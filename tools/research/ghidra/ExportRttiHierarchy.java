// Exports bounded MSVC RTTI inheritance relationships for classes containing a requested base.
// @category ERNativeUI

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Namespace;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;

public class ExportRttiHierarchy extends GhidraScript {
    private static final int MAX_TYPE_NAME_BYTES = 512;
    private static final int MAX_SYMBOLS_AT_ADDRESS = 32;
    private static final int MAX_SANE_BASE_COUNT = 4096;

    private static final class TypeInfo {
        Address address;
        String namespaceName = "";
        String symbols = "";
        String decoratedName = "";
    }

    private static final class BaseInfo {
        int index;
        Address descriptorAddress;
        TypeInfo type;
        int containedBases;
        int memberDisplacement;
        int vbtableDisplacement;
        int virtualBaseDisplacement;
        int attributes;
    }

    private static final class LocatorInfo {
        Address address;
        String symbolName;
        TypeInfo type;
        Address hierarchyAddress;
        int objectOffset;
        int constructorDisplacement;
        int declaredBaseCount;
        final List<BaseInfo> bases = new ArrayList<>();
        boolean basesTruncated;
        String vftables = "";
    }

    private Address imageBase;
    private int pointerSize;

    private static String csv(String value) {
        if (value == null) return "";
        return "\"" + value.replace("\"", "\"\"").replace("\r", "\\r")
            .replace("\n", "\\n") + "\"";
    }

    private static int positiveInt(String value, String name) {
        int parsed = Integer.parseInt(value);
        if (parsed <= 0) throw new IllegalArgumentException(name + " must be positive");
        return parsed;
    }

    private static String normalizedName(String value) {
        String lower = value.toLowerCase(Locale.ROOT);
        StringBuilder result = new StringBuilder(lower.length());
        for (int index = 0; index < lower.length(); ++index) {
            char character = lower.charAt(index);
            if ((character >= 'a' && character <= 'z') ||
                (character >= '0' && character <= '9')) {
                result.append(character);
            }
        }
        return result.toString();
    }

    private String namespaceName(Namespace namespace) {
        if (namespace == null || namespace.equals(currentProgram.getGlobalNamespace())) return "";
        List<String> components = new ArrayList<>();
        Namespace current = namespace;
        while (current != null && !current.equals(currentProgram.getGlobalNamespace())) {
            components.add(current.getName());
            current = current.getParentNamespace();
        }
        Collections.reverse(components);
        return String.join("::", components);
    }

    private String rva(Address address) {
        if (address == null || !currentProgram.getMemory().contains(address) ||
            !address.getAddressSpace().equals(imageBase.getAddressSpace())) {
            return "";
        }
        return String.format("0x%X", address.subtract(imageBase));
    }

    private String symbolsAt(Address address) {
        if (address == null) return "";
        List<String> names = new ArrayList<>();
        SymbolIterator iterator = currentProgram.getSymbolTable().getSymbolsAsIterator(address);
        while (iterator.hasNext() && names.size() < MAX_SYMBOLS_AT_ADDRESS) {
            names.add(iterator.next().getName(true));
        }
        return String.join(";", names);
    }

    private Address readRttiReference(Address fieldAddress) {
        try {
            long raw = Integer.toUnsignedLong(currentProgram.getMemory().getInt(fieldAddress));
            Address result = pointerSize == 8
                ? imageBase.add(raw)
                : fieldAddress.getAddressSpace().getAddress(raw);
            return currentProgram.getMemory().contains(result) ? result : null;
        }
        catch (Exception ignored) {
            return null;
        }
    }

    private Address readPointer(Address address) throws Exception {
        long value = pointerSize == 8
            ? currentProgram.getMemory().getLong(address)
            : Integer.toUnsignedLong(currentProgram.getMemory().getInt(address));
        return address.getAddressSpace().getAddress(value);
    }

    private String readAscii(Address address, int maximumBytes) {
        if (address == null || !currentProgram.getMemory().contains(address)) return "";
        StringBuilder result = new StringBuilder();
        for (int index = 0; index < maximumBytes; ++index) {
            try {
                int value = Byte.toUnsignedInt(currentProgram.getMemory().getByte(address.add(index)));
                if (value == 0) break;
                if (value < 0x20 || value > 0x7e) return "";
                result.append((char)value);
            }
            catch (Exception exception) {
                break;
            }
        }
        return result.toString();
    }

    private TypeInfo readType(Address address, Map<Address, Symbol> typeSymbols) {
        TypeInfo result = new TypeInfo();
        result.address = address;
        result.symbols = symbolsAt(address);
        result.decoratedName = readAscii(address.add((long)pointerSize * 2), MAX_TYPE_NAME_BYTES);
        Symbol symbol = typeSymbols.get(address);
        if (symbol != null) result.namespaceName = namespaceName(symbol.getParentNamespace());
        return result;
    }

    private String findVftables(Address locatorAddress) {
        Set<Address> tables = new HashSet<>();
        ReferenceIterator references = currentProgram.getReferenceManager()
            .getReferencesTo(locatorAddress);
        while (references.hasNext() && !monitor.isCancelled()) {
            Reference reference = references.next();
            Address metadataPointer = reference.getFromAddress();
            MemoryBlock block = currentProgram.getMemory().getBlock(metadataPointer);
            if (block == null || block.isExecute()) continue;
            try {
                if (!locatorAddress.equals(readPointer(metadataPointer))) continue;
                Address table = metadataPointer.add(pointerSize);
                if (currentProgram.getMemory().contains(table)) tables.add(table);
            }
            catch (Exception ignored) {
                // A malformed metadata reference is not a vftable association.
            }
        }

        List<Address> sorted = new ArrayList<>(tables);
        Collections.sort(sorted);
        List<String> values = new ArrayList<>();
        for (Address table : sorted) {
            String symbols = symbolsAt(table);
            values.add(rva(table) + (symbols.isEmpty() ? "" : "|" + symbols));
        }
        return String.join(";", values);
    }

    private LocatorInfo readLocator(Symbol symbol, Map<Address, Symbol> typeSymbols,
            int maximumBases) {
        try {
            LocatorInfo result = new LocatorInfo();
            result.address = symbol.getAddress();
            result.symbolName = symbol.getName(true);
            result.objectOffset = currentProgram.getMemory().getInt(result.address.add(4));
            result.constructorDisplacement = currentProgram.getMemory().getInt(result.address.add(8));
            Address typeAddress = readRttiReference(result.address.add(12));
            result.hierarchyAddress = readRttiReference(result.address.add(16));
            if (typeAddress == null || result.hierarchyAddress == null) return null;
            result.type = readType(typeAddress, typeSymbols);

            result.declaredBaseCount = currentProgram.getMemory()
                .getInt(result.hierarchyAddress.add(8));
            if (result.declaredBaseCount <= 0 ||
                result.declaredBaseCount > MAX_SANE_BASE_COUNT) return null;
            Address baseArray = readRttiReference(result.hierarchyAddress.add(12));
            if (baseArray == null) return null;

            int retained = Math.min(result.declaredBaseCount, maximumBases);
            for (int index = 0; index < retained; ++index) {
                Address descriptor = readRttiReference(baseArray.add((long)index * 4));
                if (descriptor == null) continue;
                Address baseTypeAddress = readRttiReference(descriptor);
                if (baseTypeAddress == null) continue;

                BaseInfo base = new BaseInfo();
                base.index = index;
                base.descriptorAddress = descriptor;
                base.type = readType(baseTypeAddress, typeSymbols);
                base.containedBases = currentProgram.getMemory().getInt(descriptor.add(4));
                base.memberDisplacement = currentProgram.getMemory().getInt(descriptor.add(8));
                base.vbtableDisplacement = currentProgram.getMemory().getInt(descriptor.add(12));
                base.virtualBaseDisplacement = currentProgram.getMemory().getInt(descriptor.add(16));
                base.attributes = currentProgram.getMemory().getInt(descriptor.add(20));
                result.bases.add(base);
            }
            result.basesTruncated = result.declaredBaseCount > retained;
            result.vftables = findVftables(result.address);
            return result;
        }
        catch (Exception exception) {
            return null;
        }
    }

    private boolean matches(LocatorInfo locator, String requestedBase) {
        for (BaseInfo base : locator.bases) {
            if (base.type.namespaceName.equalsIgnoreCase(requestedBase) ||
                base.type.decoratedName.equalsIgnoreCase(requestedBase)) return true;
        }
        return false;
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 4) {
            throw new IllegalArgumentException(
                "Expected: output_directory max_classes_per_filter max_bases_per_class " +
                "base_filter [base_filter ...]");
        }

        File outputDirectory = new File(args[0]);
        outputDirectory.mkdirs();
        int maxClasses = positiveInt(args[1], "max_classes_per_filter");
        int maxBases = positiveInt(args[2], "max_bases_per_class");
        imageBase = currentProgram.getImageBase();
        pointerSize = currentProgram.getDefaultPointerSize();
        if (pointerSize != 4 && pointerSize != 8) {
            throw new IllegalStateException("MSVC RTTI query requires a 32-bit or 64-bit program");
        }

        Map<Address, Symbol> typeSymbols = new HashMap<>();
        List<Symbol> locatorSymbols = new ArrayList<>();
        SymbolIterator symbols = currentProgram.getSymbolTable().getAllSymbols(true);
        while (symbols.hasNext() && !monitor.isCancelled()) {
            Symbol symbol = symbols.next();
            String simple = symbol.getName();
            String normalized = normalizedName(simple);
            if (normalized.contains("rtticompleteobjectlocator")) {
                locatorSymbols.add(symbol);
            }
            if (normalized.contains("typedescriptor") || simple.startsWith("??_R0")) {
                Symbol existing = typeSymbols.get(symbol.getAddress());
                if (existing == null || symbol.isPrimary()) {
                    typeSymbols.put(symbol.getAddress(), symbol);
                }
            }
        }
        Collections.sort(locatorSymbols, Comparator.comparing(Symbol::getAddress));

        List<LocatorInfo> locators = new ArrayList<>();
        Set<Address> seen = new HashSet<>();
        for (Symbol symbol : locatorSymbols) {
            if (!seen.add(symbol.getAddress())) continue;
            LocatorInfo locator = readLocator(symbol, typeSymbols, maxBases);
            if (locator != null) locators.add(locator);
        }

        File classesFile = new File(outputDirectory, "classes.csv");
        File basesFile = new File(outputDirectory, "base_classes.csv");
        File queriesFile = new File(outputDirectory, "queries.csv");
        try (BufferedWriter classes = new BufferedWriter(new FileWriter(classesFile));
             BufferedWriter bases = new BufferedWriter(new FileWriter(basesFile));
             BufferedWriter queries = new BufferedWriter(new FileWriter(queriesFile))) {
            classes.write("query,derived_namespace,derived_type_rva,decorated_type_name," +
                "locator_rva,locator_symbol,object_offset,constructor_displacement," +
                "hierarchy_descriptor_rva,declared_base_count,bases_retained,bases_truncated," +
                "vftables\n");
            bases.write("query,derived_type_rva,locator_rva,base_index,base_descriptor_rva," +
                "base_namespace,base_type_rva,decorated_base_name,contained_bases," +
                "member_displacement,vbtable_displacement,virtual_base_displacement," +
                "attributes\n");
            queries.write("query,total_matching_locators,locators_retained,locators_truncated\n");

            for (int argument = 3; argument < args.length && !monitor.isCancelled(); ++argument) {
                String query = args[argument];
                int matching = 0;
                int retained = 0;
                for (LocatorInfo locator : locators) {
                    if (!matches(locator, query)) continue;
                    ++matching;
                    if (retained >= maxClasses) continue;

                    classes.write(String.format(
                        "%s,%s,%s,%s,%s,%s,%d,%d,%s,%d,%d,%s,%s%n",
                        csv(query), csv(locator.type.namespaceName), rva(locator.type.address),
                        csv(locator.type.decoratedName), rva(locator.address),
                        csv(locator.symbolName), locator.objectOffset,
                        locator.constructorDisplacement, rva(locator.hierarchyAddress),
                        locator.declaredBaseCount, locator.bases.size(), locator.basesTruncated,
                        csv(locator.vftables)));

                    for (BaseInfo base : locator.bases) {
                        bases.write(String.format(
                            "%s,%s,%s,%d,%s,%s,%s,%s,%d,%d,%d,%d,0x%X%n",
                            csv(query), rva(locator.type.address), rva(locator.address), base.index,
                            rva(base.descriptorAddress), csv(base.type.namespaceName),
                            rva(base.type.address), csv(base.type.decoratedName),
                            base.containedBases, base.memberDisplacement,
                            base.vbtableDisplacement, base.virtualBaseDisplacement,
                            base.attributes));
                    }
                    ++retained;
                }
                queries.write(String.format("%s,%d,%d,%s%n", csv(query), matching, retained,
                    matching > retained));
            }
        }

        println("Exported bounded MSVC RTTI inheritance map to " +
            outputDirectory.getAbsolutePath());
    }
}
