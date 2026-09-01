// Exports the discovered function index without decompiling game code.
// @category ERNativeUI

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;

public class ExportFunctionMap extends GhidraScript {
    private static String csv(String value) {
        if (value == null) return "";
        return "\"" + value.replace("\"", "\"\"") + "\"";
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) throw new IllegalArgumentException("Expected output CSV path");
        File output = new File(args[0]);
        output.getParentFile().mkdirs();
        long imageBase = currentProgram.getImageBase().getOffset();

        try (BufferedWriter writer = new BufferedWriter(new FileWriter(output))) {
            writer.write("rva,name,body_size,is_thunk,symbol_source\n");
            FunctionIterator functions = currentProgram.getFunctionManager().getFunctions(true);
            while (functions.hasNext() && !monitor.isCancelled()) {
                Function function = functions.next();
                long rva = function.getEntryPoint().getOffset() - imageBase;
                writer.write(String.format("0x%X,%s,%d,%s,%s%n",
                    rva,
                    csv(function.getName()),
                    function.getBody().getNumAddresses(),
                    function.isThunk(),
                    csv(function.getSymbol().getSource().toString())));
            }
        }
        println("Exported function map to " + output.getAbsolutePath());
    }
}
