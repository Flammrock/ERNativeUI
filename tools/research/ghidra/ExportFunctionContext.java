// Writes a compact, human-readable context report for selected function RVAs.
// @category ERNativeUI

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.Symbol;

public class ExportFunctionContext extends GhidraScript {
    private Address imageBase;

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
            return "external";
        }
        return String.format("0x%X", address.subtract(imageBase));
    }

    private static String markdown(String value) {
        if (value == null) return "";
        return value.replace("|", "\\|").replace("\r", "").replace("\n", "\\n");
    }

    private static List<Function> sorted(Set<Function> functions) {
        List<Function> result = new ArrayList<>(functions);
        Collections.sort(result, Comparator.comparing(Function::getEntryPoint));
        return result;
    }

    private void writeFunctions(BufferedWriter writer, String heading, Set<Function> values,
            int maxReferences) throws Exception {
        writer.write("### " + heading + "\n\n");
        writer.write("| RVA | Name |\n| --- | --- |\n");
        int count = 0;
        for (Function function : sorted(values)) {
            if (count++ >= maxReferences) break;
            writer.write("| `" + rva(function.getEntryPoint()) + "` | `" +
                markdown(function.getName(true)) + "` |\n");
        }
        if (count == 0) writer.write("| - | - |\n");
        if (values.size() > maxReferences) {
            writer.write("\n_Truncated: " + values.size() + " total entries._\n");
        }
        writer.write("\n");
    }

    private void writeReferences(BufferedWriter writer, Function function, int maxReferences)
            throws Exception {
        writer.write("### Outgoing references\n\n");
        writer.write("| From RVA | Type | Target RVA | Target |\n| --- | --- | --- | --- |\n");
        int count = 0;
        Set<String> seen = new HashSet<>();
        InstructionIterator instructions = currentProgram.getListing().getInstructions(function.getBody(), true);
        while (instructions.hasNext() && count < maxReferences && !monitor.isCancelled()) {
            Instruction instruction = instructions.next();
            for (Reference reference : instruction.getReferencesFrom()) {
                Address target = reference.getToAddress();
                String key = instruction.getAddress() + ":" + target + ":" + reference.getReferenceType();
                if (!seen.add(key)) continue;

                String targetDescription = "";
                Symbol symbol = currentProgram.getSymbolTable().getPrimarySymbol(target);
                if (symbol != null) targetDescription = symbol.getName(true);
                Data data = currentProgram.getListing().getDataContaining(target);
                if (data != null && data.getValue() instanceof String) {
                    targetDescription = "string: " + (String)data.getValue();
                }
                writer.write("| `" + rva(instruction.getAddress()) + "` | " +
                    markdown(reference.getReferenceType().getName()) + " | `" + rva(target) + "` | " +
                    markdown(targetDescription) + " |\n");
                if (++count >= maxReferences) break;
            }
        }
        if (count == 0) writer.write("| - | - | - | - |\n");
        writer.write("\n");
    }

    private void writeAssembly(BufferedWriter writer, Function function, int maxInstructions)
            throws Exception {
        writer.write("### Entry assembly\n\n```asm\n");
        InstructionIterator instructions = currentProgram.getListing().getInstructions(function.getBody(), true);
        int count = 0;
        while (instructions.hasNext() && count < maxInstructions && !monitor.isCancelled()) {
            Instruction instruction = instructions.next();
            writer.write(String.format("%-12s %s%n", rva(instruction.getAddress()), instruction.toString()));
            ++count;
        }
        if (instructions.hasNext()) writer.write("; ... truncated ...\n");
        writer.write("```\n\n");
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 4) {
            throw new IllegalArgumentException(
                "Expected: output.md max_instructions max_references seed_rva [seed_rva ...]");
        }

        File output = new File(args[0]);
        File parent = output.getParentFile();
        if (parent != null) parent.mkdirs();
        int maxInstructions = positiveInt(args[1], "max_instructions");
        int maxReferences = positiveInt(args[2], "max_references");
        imageBase = currentProgram.getImageBase();

        try (BufferedWriter writer = new BufferedWriter(new FileWriter(output))) {
            writer.write("# Focused function context\n\n");
            writer.write("Program: `" + markdown(currentProgram.getName()) + "`  \n");
            writer.write("Image base: `" + imageBase + "`\n\n");

            for (int argument = 3; argument < args.length && !monitor.isCancelled(); ++argument) {
                String requested = args[argument];
                Address address = imageBase.add(parseRva(requested));
                Function function = currentProgram.getFunctionManager().getFunctionContaining(address);
                if (function == null) {
                    writer.write("## Unresolved `" + markdown(requested) + "`\n\nNo containing function.\n\n");
                    continue;
                }

                writer.write("## `" + rva(function.getEntryPoint()) + "` `" +
                    markdown(function.getName(true)) + "`\n\n");
                writer.write("- Requested RVA: `" + markdown(requested) + "`\n");
                writer.write("- Body size: `" + function.getBody().getNumAddresses() + "` bytes\n");
                writer.write("- Thunk: `" + function.isThunk() + "`\n");
                writer.write("- Signature: `" + markdown(function.getPrototypeString(true, true)) + "`\n\n");
                writeFunctions(writer, "Direct callers", function.getCallingFunctions(monitor), maxReferences);
                writeFunctions(writer, "Direct callees", function.getCalledFunctions(monitor), maxReferences);
                writeReferences(writer, function, maxReferences);
                writeAssembly(writer, function, maxInstructions);
            }
        }

        println("Exported focused function context to " + output.getAbsolutePath());
    }
}
