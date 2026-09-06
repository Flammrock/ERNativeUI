// Applies ERNativeUI's curated build-specific symbols and evidence comments.
// @category ERNativeUI

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.CommentType;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.symbol.SourceType;
import ghidra.program.model.symbol.Symbol;

public class ApplyKnownSymbols extends GhidraScript {
    private static final String NOTE_PREFIX = "ERNativeUI analytical seed";

    private static List<String> parseCsvLine(String line) {
        List<String> fields = new ArrayList<>();
        StringBuilder field = new StringBuilder();
        boolean quoted = false;
        for (int i = 0; i < line.length(); ++i) {
            char ch = line.charAt(i);
            if (quoted) {
                if (ch == '"') {
                    if (i + 1 < line.length() && line.charAt(i + 1) == '"') {
                        field.append('"');
                        ++i;
                    } else {
                        quoted = false;
                    }
                } else {
                    field.append(ch);
                }
            } else if (ch == '"') {
                quoted = true;
            } else if (ch == ',') {
                fields.add(field.toString());
                field.setLength(0);
            } else {
                field.append(ch);
            }
        }
        if (quoted) throw new IllegalArgumentException("Unterminated quoted CSV field");
        fields.add(field.toString());
        return fields;
    }

    private static long parseRva(String value) {
        String text = value.trim();
        if (text.startsWith("0x") || text.startsWith("0X")) text = text.substring(2);
        return Long.parseUnsignedLong(text, 16);
    }

    private static boolean parseStrictBoolean(String value) {
        if ("true".equalsIgnoreCase(value)) return true;
        if ("false".equalsIgnoreCase(value)) return false;
        throw new IllegalArgumentException("Expected true or false, got: " + value);
    }

    private static String field(List<String> values, Map<String, Integer> columns, String name) {
        Integer index = columns.get(name);
        if (index == null || index >= values.size()) {
            throw new IllegalArgumentException("Missing CSV column: " + name);
        }
        return values.get(index);
    }

    private void applyFunction(Address address, String name) throws Exception {
        FunctionManager functions = currentProgram.getFunctionManager();
        Function function = functions.getFunctionAt(address);
        if (function == null) {
            Function containing = functions.getFunctionContaining(address);
            if (containing != null) {
                throw new IllegalStateException(
                    "known entry lies inside existing function " + containing.getName());
            }
            function = createFunction(address, name);
            if (function == null) throw new IllegalStateException("could not create function");
        }
        function.setName(name, SourceType.USER_DEFINED);
    }

    private void applyLabel(Address address, String name) throws Exception {
        Symbol primary = currentProgram.getSymbolTable().getPrimarySymbol(address);
        if (primary != null && name.equals(primary.getName())) return;
        currentProgram.getSymbolTable().createLabel(address, name, SourceType.USER_DEFINED);
    }

    private void addEvidenceComment(
            Address address,
            String category,
            String confidence,
            String role,
            String evidence,
            String sources) {
        Listing listing = currentProgram.getListing();
        String note = NOTE_PREFIX + "\n" +
            "category: " + category + "\n" +
            "confidence: " + confidence + "\n" +
            "role: " + role + "\n" +
            "evidence: " + evidence + "\n" +
            "sources: " + sources;
        String existing = listing.getComment(CommentType.PLATE, address);
        if (existing != null && existing.contains(NOTE_PREFIX)) {
            int seedStart = existing.indexOf(NOTE_PREFIX);
            int seedEnd = existing.indexOf("\n\n", seedStart);
            String before = existing.substring(0, seedStart).trim();
            String after = seedEnd < 0
                ? ""
                : existing.substring(seedEnd + 2).trim();
            String preserved = before;
            if (!after.isBlank()) {
                preserved += (preserved.isBlank() ? "" : "\n\n") + after;
            }
            existing = preserved;
        }
        if (existing != null && !existing.isBlank()) note += "\n\n" + existing;
        listing.setComment(address, CommentType.PLATE, note);
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) {
            throw new IllegalArgumentException("Expected one known-symbol CSV path");
        }

        File input = new File(args[0]);
        String programHash = currentProgram.getExecutableSHA256();
        if (programHash == null || programHash.isBlank()) {
            throw new IllegalStateException("Imported program has no executable SHA-256");
        }
        programHash = programHash.toUpperCase(Locale.ROOT);

        int applied = 0;
        int documentary = 0;
        int failed = 0;
        try (BufferedReader reader = new BufferedReader(new FileReader(input))) {
            String headerLine = reader.readLine();
            if (headerLine == null) throw new IllegalArgumentException("Empty CSV file");
            List<String> header = parseCsvLine(headerLine);
            Map<String, Integer> columns = new HashMap<>();
            for (int i = 0; i < header.size(); ++i) columns.put(header.get(i), i);

            String line;
            int lineNumber = 1;
            while ((line = reader.readLine()) != null && !monitor.isCancelled()) {
                ++lineNumber;
                if (line.isBlank()) continue;
                try {
                    List<String> values = parseCsvLine(line);
                    String expectedHash = field(values, columns, "image_sha256")
                        .toUpperCase(Locale.ROOT);
                    if (!programHash.equals(expectedHash)) {
                        throw new IllegalStateException(
                            "CSV is for SHA-256 " + expectedHash +
                            " but the imported program is " + programHash);
                    }
                    if (!parseStrictBoolean(field(values, columns, "apply_name"))) {
                        ++documentary;
                        continue;
                    }

                    long rva = parseRva(field(values, columns, "rva"));
                    Address address = currentProgram.getImageBase().add(rva);
                    if (!currentProgram.getMemory().contains(address)) {
                        throw new IllegalStateException("RVA is outside imported memory");
                    }
                    String name = field(values, columns, "analytical_name");
                    String kind = field(values, columns, "symbol_kind");
                    if ("function".equals(kind)) {
                        applyFunction(address, name);
                    } else {
                        applyLabel(address, name);
                    }
                    addEvidenceComment(
                        address,
                        field(values, columns, "category"),
                        field(values, columns, "confidence"),
                        field(values, columns, "production_role"),
                        field(values, columns, "evidence"),
                        field(values, columns, "sources"));
                    ++applied;
                } catch (Exception error) {
                    ++failed;
                    printerr("Known-symbol line " + lineNumber + " failed: " + error.getMessage());
                }
            }
        }

        println("Known-symbol import: applied=" + applied +
            " documentary=" + documentary + " failed=" + failed);
        if (failed != 0) throw new IllegalStateException("One or more known symbols failed");
    }
}
