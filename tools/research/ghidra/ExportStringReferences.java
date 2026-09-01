// Exports defined string values and their references without bulk decompilation.
// @category ERNativeUI

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.DataIterator;
import ghidra.program.model.symbol.Reference;

public class ExportStringReferences extends GhidraScript {
    private static String csv(String value) {
        if (value == null) return "";
        return "\"" + value.replace("\"", "\"\"").replace("\r", "\\r").replace("\n", "\\n") + "\"";
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) throw new IllegalArgumentException("Expected output CSV path");
        File output = new File(args[0]);
        output.getParentFile().mkdirs();
        long imageBase = currentProgram.getImageBase().getOffset();

        try (BufferedWriter writer = new BufferedWriter(new FileWriter(output))) {
            writer.write("string_rva,value,reference_rva,reference_type\n");
            DataIterator values = currentProgram.getListing().getDefinedData(true);
            while (values.hasNext() && !monitor.isCancelled()) {
                Data data = values.next();
                Object value = data.getValue();
                if (!(value instanceof String)) continue;
                long stringRva = data.getAddress().getOffset() - imageBase;
                Reference[] references = getReferencesTo(data.getAddress());
                if (references.length == 0) {
                    writer.write(String.format("0x%X,%s,,%n", stringRva, csv((String)value)));
                    continue;
                }
                for (Reference reference : references) {
                    long referenceRva = reference.getFromAddress().getOffset() - imageBase;
                    writer.write(String.format("0x%X,%s,0x%X,%s%n",
                        stringRva,
                        csv((String)value),
                        referenceRva,
                        csv(reference.getReferenceType().getName())));
                }
            }
        }
        println("Exported string references to " + output.getAbsolutePath());
    }
}
