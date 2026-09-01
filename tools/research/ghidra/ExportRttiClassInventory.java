// Exports a bounded, read-only inventory of MSVC RTTI classes and recovered vftables.
// @category ERNativeUI

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Namespace;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;

public class ExportRttiClassInventory extends GhidraScript {
    private static final int MAX_TYPE_NAME_BYTES = 512;
    private static final int MAX_SYMBOLS_AT_ADDRESS = 32;

    private static final class TypeInfo {
        Address address;
        String symbols = "";
        String dataType = "";
        String decoratedName = "";
        String namespaceName = "";
    }

    private static final class LocatorInfo {
        Address address;
        String symbolName;
        String namespaceName;
        TypeInfo type;
        Address hierarchyAddress;
        Integer objectOffset;
        Integer constructorDisplacement;
    }

    private static final class ClassInfo {
        String key;
        String namespaceName;
        TypeInfo type;
        Address sortAddress;
        final List<LocatorInfo> locators = new ArrayList<>();
    }

    private static final class VtableInfo {
        Address address;
        Address locatorAddress;
        String association;
        String symbols;
        boolean recoveredSymbol;
    }

    private static final class SlotStats {
        int retained;
        boolean truncated;
        String stopReason;
    }

    private static final class VtableCollection {
        final List<VtableInfo> tables = new ArrayList<>();
        boolean truncated;
    }

    private Address imageBase;
    private int pointerSize;
    private BufferedWriter slotsWriter;

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

    private String rva(Address address) {
        if (address == null || !currentProgram.getMemory().contains(address) ||
            !address.getAddressSpace().equals(imageBase.getAddressSpace())) {
            return "";
        }
        return String.format("0x%X", address.subtract(imageBase));
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

    private String symbolsAt(Address address) {
        if (address == null) return "";
        List<String> names = new ArrayList<>();
        SymbolIterator iterator = currentProgram.getSymbolTable().getSymbolsAsIterator(address);
        while (iterator.hasNext() && names.size() < MAX_SYMBOLS_AT_ADDRESS) {
            names.add(iterator.next().getName(true));
        }
        return String.join(";", names);
    }

    private Symbol preferredSymbol(Map<Address, Symbol> symbols, Symbol candidate) {
        Symbol existing = symbols.get(candidate.getAddress());
        if (existing == null || candidate.isPrimary()) return candidate;
        return existing;
    }

    private Address readRttiReference(Address fieldAddress) {
        try {
            long raw = Integer.toUnsignedLong(currentProgram.getMemory().getInt(fieldAddress));
            Address result;
            if (pointerSize == 8) {
                result = imageBase.add(raw);
            }
            else {
                result = fieldAddress.getAddressSpace().getAddress(raw);
            }
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

    private TypeInfo readTypeInfo(Address address, Map<Address, Symbol> typeSymbols) {
        if (address == null) return null;
        TypeInfo result = new TypeInfo();
        result.address = address;
        result.symbols = symbolsAt(address);
        Data data = currentProgram.getListing().getDataAt(address);
        result.dataType = data == null ? "" : data.getDataType().getName();
        result.decoratedName = readAscii(address.add((long)pointerSize * 2), MAX_TYPE_NAME_BYTES);
        Symbol symbol = typeSymbols.get(address);
        if (symbol != null) result.namespaceName = namespaceName(symbol.getParentNamespace());
        return result;
    }

    private LocatorInfo readLocator(Symbol symbol, Map<Address, Symbol> typeSymbols) {
        LocatorInfo result = new LocatorInfo();
        result.address = symbol.getAddress();
        result.symbolName = symbol.getName(true);
        result.namespaceName = namespaceName(symbol.getParentNamespace());
        try {
            result.objectOffset = currentProgram.getMemory().getInt(result.address.add(4));
            result.constructorDisplacement = currentProgram.getMemory().getInt(result.address.add(8));
        }
        catch (Exception ignored) {
            // The row remains useful even when the structure is only partly recovered.
        }
        Address typeAddress = readRttiReference(result.address.add(12));
        result.type = readTypeInfo(typeAddress, typeSymbols);
        result.hierarchyAddress = readRttiReference(result.address.add(16));
        if (result.namespaceName.isEmpty() && result.type != null) {
            result.namespaceName = result.type.namespaceName;
        }
        return result;
    }

    private String classKey(LocatorInfo locator) {
        if (locator.type != null && locator.type.address != null) {
            return (locator.namespaceName.isEmpty() ? "<anonymous>" : locator.namespaceName) +
                "@" + rva(locator.type.address);
        }
        return (locator.namespaceName.isEmpty() ? "<anonymous>" : locator.namespaceName) +
            "@locator:" + rva(locator.address);
    }

    private String classHaystack(ClassInfo info) {
        StringBuilder result = new StringBuilder();
        result.append(info.key).append('\n').append(info.namespaceName);
        if (info.type != null) {
            result.append('\n').append(info.type.symbols).append('\n').append(info.type.dataType)
                .append('\n').append(info.type.decoratedName);
        }
        for (LocatorInfo locator : info.locators) {
            result.append('\n').append(locator.symbolName);
        }
        return result.toString().toLowerCase(Locale.ROOT);
    }

    private VtableCollection collectVftables(ClassInfo info, List<LocatorInfo> retainedLocators,
            Map<String, List<Symbol>> vftablesByNamespace, int maxVftables) {
        LinkedHashMap<Address, VtableInfo> candidates = new LinkedHashMap<>();
        boolean candidateLimitHit = false;
        for (LocatorInfo locator : retainedLocators) {
            ReferenceIterator references = currentProgram.getReferenceManager()
                .getReferencesTo(locator.address);
            while (references.hasNext() && !monitor.isCancelled()) {
                Reference reference = references.next();
                Address metadataPointer = reference.getFromAddress();
                MemoryBlock metadataBlock = currentProgram.getMemory().getBlock(metadataPointer);
                if (metadataBlock == null || metadataBlock.isExecute()) continue;
                try {
                    if (!locator.address.equals(readPointer(metadataPointer))) continue;
                }
                catch (Exception exception) {
                    continue;
                }
                Address table;
                try {
                    table = metadataPointer.add(pointerSize);
                }
                catch (Exception exception) {
                    continue;
                }
                if (!currentProgram.getMemory().contains(table)) continue;
                VtableInfo candidate = candidates.get(table);
                if (candidate == null) {
                    if (candidates.size() >= maxVftables) {
                        candidateLimitHit = true;
                        continue;
                    }
                    candidate = new VtableInfo();
                    candidate.address = table;
                    candidate.locatorAddress = locator.address;
                    candidate.association = "complete_object_locator_reference";
                    candidate.symbols = symbolsAt(table);
                    candidate.recoveredSymbol = !candidate.symbols.isEmpty() &&
                        normalizedName(candidate.symbols).contains("vftable");
                    candidates.put(table, candidate);
                }
            }
        }

        List<Symbol> namespaceTables = info.namespaceName.isEmpty() ? null :
            vftablesByNamespace.get(info.namespaceName);
        if (namespaceTables != null) {
            for (Symbol symbol : namespaceTables) {
                if (candidates.containsKey(symbol.getAddress())) continue;
                if (candidates.size() >= maxVftables) {
                    candidateLimitHit = true;
                    continue;
                }
                VtableInfo candidate = new VtableInfo();
                candidate.address = symbol.getAddress();
                candidate.association = "class_namespace_symbol";
                candidate.symbols = symbolsAt(candidate.address);
                candidate.recoveredSymbol = true;
                candidates.put(candidate.address, candidate);
            }
        }

        VtableCollection result = new VtableCollection();
        result.tables.addAll(candidates.values());
        Collections.sort(result.tables, Comparator.comparing(value -> value.address));

        result.truncated = candidateLimitHit;
        return result;
    }

    private SlotStats writeSlots(String query, ClassInfo info, VtableInfo table, int maxSlots)
            throws Exception {
        SlotStats stats = new SlotStats();
        stats.stopReason = "max_slots";
        int effectiveMaximum = maxSlots;
        Data tableData = currentProgram.getListing().getDataAt(table.address);
        boolean boundedByDefinedArray = tableData != null && tableData.isArray();
        if (boundedByDefinedArray) effectiveMaximum = Math.min(maxSlots, tableData.getNumComponents());

        int consecutiveNonCodeTargets = 0;
        for (int slot = 0; slot < effectiveMaximum && !monitor.isCancelled(); ++slot) {
            Address entryAddress = table.address.add((long)slot * pointerSize);
            Address target;
            try {
                target = readPointer(entryAddress);
            }
            catch (Exception exception) {
                stats.stopReason = "unreadable_entry";
                break;
            }

            Function exact = currentProgram.getFunctionManager().getFunctionAt(target);
            Function containing = exact == null
                ? currentProgram.getFunctionManager().getFunctionContaining(target) : exact;
            MemoryBlock targetBlock = currentProgram.getMemory().getBlock(target);
            boolean executable = targetBlock != null && targetBlock.isExecute();
            if (containing == null && !executable) ++consecutiveNonCodeTargets;
            else consecutiveNonCodeTargets = 0;

            slotsWriter.write(String.format(
                "%s,%s,%s,%s,%d,0x%X,%s,%s,%s,%s,%s,%s,%s%n",
                csv(query), csv(info.key), rva(table.locatorAddress), rva(table.address), slot,
                (long)slot * pointerSize, rva(target), csv(symbolsAt(target)),
                containing == null ? "" : rva(containing.getEntryPoint()),
                csv(containing == null ? "" : containing.getName(true)),
                exact != null,
                containing != null && containing.isThunk(),
                executable));
            ++stats.retained;

            if (consecutiveNonCodeTargets >= 2) {
                stats.stopReason = "two_consecutive_non_code_targets";
                break;
            }
        }

        if (monitor.isCancelled()) stats.stopReason = "cancelled";
        else if (boundedByDefinedArray && effectiveMaximum < maxSlots &&
                 stats.retained >= effectiveMaximum) stats.stopReason = "defined_array_end";
        stats.truncated = stats.stopReason.equals("max_slots") && stats.retained >= maxSlots;
        return stats;
    }

    private void writeClass(BufferedWriter classes, BufferedWriter locators,
            BufferedWriter vftables, String query, ClassInfo info, int maxLocators,
            int maxVftables, int maxSlots, Map<String, List<Symbol>> vftablesByNamespace)
            throws Exception {
        int retainedLocatorCount = Math.min(info.locators.size(), maxLocators);
        List<LocatorInfo> retainedLocators =
            new ArrayList<>(info.locators.subList(0, retainedLocatorCount));
        VtableCollection tableCollection = collectVftables(info, retainedLocators, vftablesByNamespace,
            maxVftables);

        TypeInfo type = info.type;
        classes.write(String.format(
            "%s,%s,%s,%s,%s,%s,%s,%d,%d,%s,%d,%s%n",
            csv(query), csv(info.key), csv(info.namespaceName),
            type == null ? "" : rva(type.address),
            csv(type == null ? "" : type.symbols),
            csv(type == null ? "" : type.dataType),
            csv(type == null ? "" : type.decoratedName),
            info.locators.size(), retainedLocatorCount, info.locators.size() > retainedLocatorCount,
            tableCollection.tables.size(), tableCollection.truncated));

        for (LocatorInfo locator : retainedLocators) {
            locators.write(String.format(
                "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s%n",
                csv(query), csv(info.key), rva(locator.address), csv(locator.symbolName),
                locator.objectOffset == null ? "" : locator.objectOffset.toString(),
                locator.constructorDisplacement == null ? "" :
                    locator.constructorDisplacement.toString(),
                rva(locator.hierarchyAddress),
                locator.type == null ? "" : rva(locator.type.address),
                csv(locator.type == null ? "" : locator.type.symbols),
                csv(locator.type == null ? "" : locator.type.decoratedName)));
        }

        for (VtableInfo table : tableCollection.tables) {
            SlotStats stats = writeSlots(query, info, table, maxSlots);
            vftables.write(String.format(
                "%s,%s,%s,%s,%s,%s,%s,%d,%s,%s%n",
                csv(query), csv(info.key), rva(table.locatorAddress), rva(table.address),
                csv(table.symbols), csv(table.association), table.recoveredSymbol,
                stats.retained, stats.truncated, csv(stats.stopReason)));
        }
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 6) {
            throw new IllegalArgumentException(
                "Expected: output_directory max_classes_per_filter max_locators_per_class " +
                "max_vftables_per_class max_slots_per_vftable filter [filter ...]");
        }

        File outputDirectory = new File(args[0]);
        outputDirectory.mkdirs();
        int maxClasses = positiveInt(args[1], "max_classes_per_filter");
        int maxLocators = positiveInt(args[2], "max_locators_per_class");
        int maxVftables = positiveInt(args[3], "max_vftables_per_class");
        int maxSlots = positiveInt(args[4], "max_slots_per_vftable");
        imageBase = currentProgram.getImageBase();
        pointerSize = currentProgram.getDefaultPointerSize();
        if (pointerSize != 4 && pointerSize != 8) {
            throw new IllegalStateException("MSVC RTTI query requires a 32-bit or 64-bit program");
        }

        Map<Address, Symbol> locatorSymbols = new HashMap<>();
        Map<Address, Symbol> typeSymbols = new HashMap<>();
        Map<String, List<Symbol>> vftablesByNamespace = new HashMap<>();
        SymbolIterator symbols = currentProgram.getSymbolTable().getAllSymbols(true);
        while (symbols.hasNext() && !monitor.isCancelled()) {
            Symbol symbol = symbols.next();
            String simpleName = symbol.getName();
            String normalized = normalizedName(simpleName);
            if (normalized.contains("rtticompleteobjectlocator")) {
                locatorSymbols.put(symbol.getAddress(), preferredSymbol(locatorSymbols, symbol));
            }
            if (normalized.contains("typedescriptor") || simpleName.startsWith("??_R0")) {
                typeSymbols.put(symbol.getAddress(), preferredSymbol(typeSymbols, symbol));
            }
            if (normalized.contains("vftable") && !normalized.contains("vftablemetaptr")) {
                String namespace = namespaceName(symbol.getParentNamespace());
                vftablesByNamespace.computeIfAbsent(namespace, unused -> new ArrayList<>())
                    .add(symbol);
            }
        }
        for (List<Symbol> namespaceTables : vftablesByNamespace.values()) {
            Collections.sort(namespaceTables, Comparator.comparing(Symbol::getAddress));
        }

        Map<String, ClassInfo> classMap = new LinkedHashMap<>();
        Set<Address> representedTypeAddresses = new HashSet<>();
        List<Symbol> sortedLocators = new ArrayList<>(locatorSymbols.values());
        Collections.sort(sortedLocators, Comparator.comparing(Symbol::getAddress));
        for (Symbol symbol : sortedLocators) {
            LocatorInfo locator = readLocator(symbol, typeSymbols);
            String key = classKey(locator);
            ClassInfo info = classMap.get(key);
            if (info == null) {
                info = new ClassInfo();
                info.key = key;
                info.namespaceName = locator.namespaceName;
                info.type = locator.type;
                info.sortAddress = locator.type == null ? locator.address : locator.type.address;
                classMap.put(key, info);
            }
            info.locators.add(locator);
            if (locator.type != null && locator.type.address != null) {
                representedTypeAddresses.add(locator.type.address);
            }
        }

        for (Map.Entry<Address, Symbol> entry : typeSymbols.entrySet()) {
            if (representedTypeAddresses.contains(entry.getKey())) continue;
            TypeInfo type = readTypeInfo(entry.getKey(), typeSymbols);
            ClassInfo info = new ClassInfo();
            info.namespaceName = type.namespaceName;
            info.type = type;
            info.sortAddress = type.address;
            info.key = (info.namespaceName.isEmpty() ? "<anonymous>" : info.namespaceName) +
                "@" + rva(type.address);
            classMap.putIfAbsent(info.key, info);
        }

        List<ClassInfo> classes = new ArrayList<>(classMap.values());
        Collections.sort(classes, Comparator.comparing(value -> value.sortAddress));

        File classesFile = new File(outputDirectory, "classes.csv");
        File locatorsFile = new File(outputDirectory, "complete_object_locators.csv");
        File vftablesFile = new File(outputDirectory, "vftables.csv");
        File slotsFile = new File(outputDirectory, "virtual_slots.csv");
        File summaryFile = new File(outputDirectory, "queries.csv");
        try (BufferedWriter classWriter = new BufferedWriter(new FileWriter(classesFile));
             BufferedWriter locatorWriter = new BufferedWriter(new FileWriter(locatorsFile));
             BufferedWriter vftableWriter = new BufferedWriter(new FileWriter(vftablesFile));
             BufferedWriter openedSlots = new BufferedWriter(new FileWriter(slotsFile));
             BufferedWriter summaryWriter = new BufferedWriter(new FileWriter(summaryFile))) {
            slotsWriter = openedSlots;
            classWriter.write("query,class_key,class_namespace,type_descriptor_rva," +
                "type_descriptor_symbols,type_descriptor_data_type,decorated_type_name," +
                "locator_count,locators_retained,locators_truncated,vftables_retained," +
                "vftables_truncated\n");
            locatorWriter.write("query,class_key,locator_rva,locator_symbol,object_offset," +
                "constructor_displacement,hierarchy_descriptor_rva,type_descriptor_rva," +
                "type_descriptor_symbols,decorated_type_name\n");
            vftableWriter.write("query,class_key,locator_rva,vftable_rva,vftable_symbols," +
                "association,vftable_symbol_recovered,slots_retained,slots_truncated," +
                "stop_reason\n");
            slotsWriter.write("query,class_key,locator_rva,vftable_rva,slot,byte_offset," +
                "entry_rva,entry_symbols,function_rva,function_name,is_exact_function_entry," +
                "is_thunk,is_executable\n");
            summaryWriter.write("query,total_matching_classes,classes_retained,classes_truncated\n");

            for (int argument = 5; argument < args.length && !monitor.isCancelled(); ++argument) {
                String query = args[argument];
                String needle = query.toLowerCase(Locale.ROOT);
                int matching = 0;
                int retained = 0;
                for (ClassInfo info : classes) {
                    boolean matches = query.equals("*") || classHaystack(info).contains(needle);
                    if (!matches) continue;
                    ++matching;
                    if (retained >= maxClasses) continue;
                    writeClass(classWriter, locatorWriter, vftableWriter, query, info,
                        maxLocators, maxVftables, maxSlots, vftablesByNamespace);
                    ++retained;
                }
                summaryWriter.write(String.format("%s,%d,%d,%s%n", csv(query), matching,
                    retained, matching > retained));
            }
        }

        println("Exported bounded MSVC RTTI inventory to " + outputDirectory.getAbsolutePath());
    }
}
