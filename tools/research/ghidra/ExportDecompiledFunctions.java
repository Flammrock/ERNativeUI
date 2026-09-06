// Decompiles only explicitly requested function RVAs into a bounded Markdown report.
// @category ERNativeUI

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;

public class ExportDecompiledFunctions extends GhidraScript {
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

    private static String markdown(String value) {
        if (value == null) return "";
        return value.replace("`", "\\`").replace("\r", "");
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 4) {
            throw new IllegalArgumentException(
                "Expected: output.md timeout_seconds max_chars_per_function seed_rva [seed_rva ...]");
        }

        File output = new File(args[0]);
        File parent = output.getParentFile();
        if (parent != null) parent.mkdirs();
        int timeoutSeconds = positiveInt(args[1], "timeout_seconds");
        int maxCharacters = positiveInt(args[2], "max_chars_per_function");
        Address imageBase = currentProgram.getImageBase();

        DecompInterface decompiler = new DecompInterface();
        DecompileOptions options = new DecompileOptions();
        decompiler.setOptions(options);
        decompiler.toggleCCode(true);
        decompiler.toggleSyntaxTree(false);
        decompiler.setSimplificationStyle("decompile");
        if (!decompiler.openProgram(currentProgram)) {
            throw new IllegalStateException("Decompiler failed to open program: " + decompiler.getLastMessage());
        }

        try (BufferedWriter writer = new BufferedWriter(new FileWriter(output))) {
            writer.write("# Focused decompilation\n\n");
            writer.write("The names and types shown here are Ghidra analysis output, not original source identifiers.\n\n");
            for (int argument = 3; argument < args.length && !monitor.isCancelled(); ++argument) {
                String requested = args[argument];
                Address address = imageBase.add(parseRva(requested));
                Function function = currentProgram.getFunctionManager().getFunctionContaining(address);
                if (function == null) {
                    writer.write("## Unresolved `" + markdown(requested) + "`\n\nNo containing function.\n\n");
                    continue;
                }

                String functionRva = String.format("0x%X", function.getEntryPoint().subtract(imageBase));
                writer.write("## `" + functionRva + "` `" + markdown(function.getName(true)) + "`\n\n");
                DecompileResults results = decompiler.decompileFunction(function, timeoutSeconds, monitor);
                if (!results.decompileCompleted() || results.getDecompiledFunction() == null) {
                    writer.write("Decompilation failed: `" + markdown(results.getErrorMessage()) + "`\n\n");
                    continue;
                }
                String code = results.getDecompiledFunction().getC();
                boolean truncated = code.length() > maxCharacters;
                if (truncated) code = code.substring(0, maxCharacters);
                writer.write("```c\n");
                writer.write(code);
                if (truncated) writer.write("\n/* ... output truncated by ERNativeUI query limit ... */");
                writer.write("\n```\n\n");
            }
        }
        finally {
            decompiler.dispose();
        }

        println("Exported focused decompilation to " + output.getAbsolutePath());
    }
}
