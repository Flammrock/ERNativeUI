// Finds a bounded set of strings and symbols, then records their direct references.
// @category ERNativeUI

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.DataIterator;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;

public class FindUiAnchors extends GhidraScript {
    private BufferedWriter writer;
    private Address imageBase;

    private static String csv(String value) {
        if (value == null) return "";
        return "\"" + value.replace("\"", "\"\"").replace("\r", "\\r").replace("\n", "\\n") + "\"";
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

    private String functionRva(Function function) {
        return function == null ? "" : rva(function.getEntryPoint());
    }

    private void writeAnchor(String query, String kind, Address address, String value,
            int maxReferences) throws Exception {
        ReferenceIterator iterator = currentProgram.getReferenceManager().getReferencesTo(address);
        List<Reference> retained = new ArrayList<>();
        while (iterator.hasNext() && retained.size() < maxReferences && !monitor.isCancelled()) {
            retained.add(iterator.next());
        }
        boolean truncated = iterator.hasNext();

        if (retained.isEmpty()) {
            writer.write(String.format("%s,%s,%s,%s,%s,0,false,,,,,%n",
                csv(query), csv(kind), rva(address), csv(address.toString()), csv(value)));
            return;
        }

        for (Reference reference : retained) {
            Address from = reference.getFromAddress();
            Function function = currentProgram.getFunctionManager().getFunctionContaining(from);
            writer.write(String.format("%s,%s,%s,%s,%s,%d,%s,%s,%s,%s,%s,%s%n",
                csv(query),
                csv(kind),
                rva(address),
                csv(address.toString()),
                csv(value),
                retained.size(),
                truncated,
                rva(from),
                csv(from.toString()),
                csv(reference.getReferenceType().getName()),
                functionRva(function),
                csv(function == null ? "" : function.getName(true))));
        }
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 4) {
            throw new IllegalArgumentException(
                "Expected: output.csv max_matches_per_query max_references_per_match query [query ...]");
        }

        File output = new File(args[0]);
        File parent = output.getParentFile();
        if (parent != null) parent.mkdirs();
        int maxMatches = positiveInt(args[1], "max_matches_per_query");
        int maxReferences = positiveInt(args[2], "max_references_per_match");
        imageBase = currentProgram.getImageBase();

        try (BufferedWriter opened = new BufferedWriter(new FileWriter(output))) {
            writer = opened;
            writer.write("query,anchor_kind,anchor_rva,anchor_address,anchor_value," +
                "retained_reference_count,references_truncated,reference_rva,reference_address," +
                "reference_type,source_function_rva,source_function_name\n");

            for (int argument = 3; argument < args.length && !monitor.isCancelled(); ++argument) {
                String query = args[argument];
                String needle = query.toLowerCase(Locale.ROOT);
                int matches = 0;

                DataIterator dataIterator = currentProgram.getListing().getDefinedData(true);
                while (dataIterator.hasNext() && matches < maxMatches && !monitor.isCancelled()) {
                    Data data = dataIterator.next();
                    Object value = data.getValue();
                    if (!(value instanceof String)) continue;
                    String text = (String)value;
                    if (!text.toLowerCase(Locale.ROOT).contains(needle)) continue;
                    writeAnchor(query, "string", data.getAddress(), text, maxReferences);
                    ++matches;
                }

                SymbolIterator symbolIterator = currentProgram.getSymbolTable().getAllSymbols(true);
                while (symbolIterator.hasNext() && matches < maxMatches && !monitor.isCancelled()) {
                    Symbol symbol = symbolIterator.next();
                    String name = symbol.getName(true);
                    if (!name.toLowerCase(Locale.ROOT).contains(needle)) continue;
                    writeAnchor(query, "symbol:" + symbol.getSymbolType(), symbol.getAddress(), name,
                        maxReferences);
                    ++matches;
                }

                if (matches == 0) {
                    writer.write(String.format("%s,%s,,,,0,false,,,,,%n", csv(query), csv("not_found")));
                }
            }
        }

        println("Exported bounded UI anchor references to " + output.getAbsolutePath());
    }
}
