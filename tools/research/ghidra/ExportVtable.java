// Resolves bounded pointer tables as candidate MSVC vftables.
// @category ERNativeUI

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Symbol;

public class ExportVtable extends GhidraScript {
    private Address imageBase;

    private static String csv(String value) {
        if (value == null) return "";
        return "\"" + value.replace("\"", "\"\"") + "\"";
    }

    private static int positiveInt(String value, String name) {
        int parsed = Integer.parseInt(value);
        if (parsed <= 0) throw new IllegalArgumentException(name + " must be positive");
        return parsed;
    }

    private static long parseRva(String value) {
        String normalized = value.trim().toLowerCase();
        if (normalized.startsWith("rva:")) normalized = normalized.substring(4);
        if (normalized.startsWith("0x")) return Long.parseUnsignedLong(normalized.substring(2), 16);
        return Long.parseUnsignedLong(normalized, 16);
    }

    private String rva(Address address) {
        if (address == null || !currentProgram.getMemory().contains(address) ||
            !address.getAddressSpace().equals(imageBase.getAddressSpace())) {
            return "";
        }
        return String.format("0x%X", address.subtract(imageBase));
    }

    private Address readPointer(Address address, int pointerSize) throws Exception {
        long value = pointerSize == 8
            ? currentProgram.getMemory().getLong(address)
            : Integer.toUnsignedLong(currentProgram.getMemory().getInt(address));
        return address.getAddressSpace().getAddress(value);
    }

    private String symbolName(Address address) {
        Symbol symbol = currentProgram.getSymbolTable().getPrimarySymbol(address);
        return symbol == null ? "" : symbol.getName(true);
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 3) {
            throw new IllegalArgumentException("Expected: output.csv max_slots vftable_rva [vftable_rva ...]");
        }

        File output = new File(args[0]);
        File parent = output.getParentFile();
        if (parent != null) parent.mkdirs();
        int maxSlots = positiveInt(args[1], "max_slots");
        int pointerSize = currentProgram.getDefaultPointerSize();
        imageBase = currentProgram.getImageBase();

        try (BufferedWriter writer = new BufferedWriter(new FileWriter(output))) {
            writer.write("vftable_rva,vftable_symbol,locator_rva,locator_symbol,slot,entry_rva," +
                "entry_symbol,function_rva,function_name\n");
            for (int argument = 2; argument < args.length && !monitor.isCancelled(); ++argument) {
                Address table = imageBase.add(parseRva(args[argument]));
                Address locatorPointer = null;
                try {
                    locatorPointer = readPointer(table.subtract(pointerSize), pointerSize);
                }
                catch (Exception ignored) {
                    // Candidate may not have a readable MSVC CompleteObjectLocator pointer.
                }

                int consecutiveUnresolved = 0;
                for (int slot = 0; slot < maxSlots && !monitor.isCancelled(); ++slot) {
                    Address entryAddress = table.add((long)slot * pointerSize);
                    Address target;
                    try {
                        target = readPointer(entryAddress, pointerSize);
                    }
                    catch (Exception exception) {
                        break;
                    }
                    Function function = currentProgram.getFunctionManager().getFunctionAt(target);
                    if (function == null) function = currentProgram.getFunctionManager().getFunctionContaining(target);
                    if (function == null) ++consecutiveUnresolved;
                    else consecutiveUnresolved = 0;

                    writer.write(String.format("%s,%s,%s,%s,%d,%s,%s,%s,%s%n",
                        rva(table),
                        csv(symbolName(table)),
                        rva(locatorPointer),
                        csv(locatorPointer == null ? "" : symbolName(locatorPointer)),
                        slot,
                        rva(target),
                        csv(symbolName(target)),
                        function == null ? "" : rva(function.getEntryPoint()),
                        csv(function == null ? "" : function.getName(true))));

                    if (consecutiveUnresolved >= 2) break;
                }
            }
        }

        println("Exported candidate vftable slots to " + output.getAbsolutePath());
    }
}
