// Exports a bounded caller/callee graph rooted at one or more function RVAs.
// @category ERNativeUI

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;

public class ExportCallGraph extends GhidraScript {
    private static final class Pending {
        final Function function;
        final int depth;

        Pending(Function function, int depth) {
            this.function = function;
            this.depth = depth;
        }
    }

    private Address imageBase;

    private static String csv(String value) {
        if (value == null) return "";
        return "\"" + value.replace("\"", "\"\"") + "\"";
    }

    private static int nonNegativeInt(String value, String name) {
        int parsed = Integer.parseInt(value);
        if (parsed < 0) throw new IllegalArgumentException(name + " must not be negative");
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

    private static List<Function> sorted(Set<Function> functions) {
        List<Function> result = new ArrayList<>(functions);
        Collections.sort(result, Comparator.comparing(Function::getEntryPoint));
        return result;
    }

    private void writeNode(BufferedWriter nodes, String seed, int depth, Function function)
            throws Exception {
        nodes.write(String.format("%s,%d,%s,%s,%d,%s,%s%n",
            csv(seed),
            depth,
            rva(function.getEntryPoint()),
            csv(function.getName(true)),
            function.getBody().getNumAddresses(),
            function.isThunk(),
            function.isExternal()));
    }

    private void writeEdge(BufferedWriter edges, String seed, String traversal, int depth,
            Function from, Function to) throws Exception {
        edges.write(String.format("%s,%s,%d,%s,%s,%s,%s%n",
            csv(seed), csv(traversal), depth,
            rva(from.getEntryPoint()), csv(from.getName(true)),
            rva(to.getEntryPoint()), csv(to.getName(true))));
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 5) {
            throw new IllegalArgumentException(
                "Expected: output_directory callers|callees|both max_depth max_nodes seed_rva [seed_rva ...]");
        }

        File outputDirectory = new File(args[0]);
        outputDirectory.mkdirs();
        String direction = args[1].toLowerCase();
        if (!direction.equals("callers") && !direction.equals("callees") && !direction.equals("both")) {
            throw new IllegalArgumentException("direction must be callers, callees, or both");
        }
        int maxDepth = nonNegativeInt(args[2], "max_depth");
        int maxNodes = nonNegativeInt(args[3], "max_nodes");
        if (maxNodes == 0) throw new IllegalArgumentException("max_nodes must be positive");
        imageBase = currentProgram.getImageBase();

        File nodesFile = new File(outputDirectory, "nodes.csv");
        File edgesFile = new File(outputDirectory, "edges.csv");
        try (BufferedWriter nodes = new BufferedWriter(new FileWriter(nodesFile));
             BufferedWriter edges = new BufferedWriter(new FileWriter(edgesFile))) {
            nodes.write("seed_rva,distance,function_rva,function_name,body_size,is_thunk,is_external\n");
            edges.write("seed_rva,traversal,distance,from_rva,from_name,to_rva,to_name\n");

            for (int argument = 4; argument < args.length && !monitor.isCancelled(); ++argument) {
                String seedText = args[argument];
                Address seedAddress = imageBase.add(parseRva(seedText));
                Function seed = currentProgram.getFunctionManager().getFunctionContaining(seedAddress);
                if (seed == null) {
                    printerr("No function contains seed RVA " + seedText);
                    continue;
                }

                String canonicalSeed = rva(seed.getEntryPoint());
                ArrayDeque<Pending> queue = new ArrayDeque<>();
                Set<Address> discovered = new HashSet<>();
                queue.add(new Pending(seed, 0));
                discovered.add(seed.getEntryPoint());

                while (!queue.isEmpty() && discovered.size() <= maxNodes && !monitor.isCancelled()) {
                    Pending pending = queue.removeFirst();
                    Function current = pending.function;
                    writeNode(nodes, canonicalSeed, pending.depth, current);
                    if (pending.depth >= maxDepth) continue;

                    if (direction.equals("callees") || direction.equals("both")) {
                        for (Function callee : sorted(current.getCalledFunctions(monitor))) {
                            boolean known = discovered.contains(callee.getEntryPoint());
                            if (!known && discovered.size() >= maxNodes) continue;
                            writeEdge(edges, canonicalSeed, "callee", pending.depth + 1, current, callee);
                            if (discovered.add(callee.getEntryPoint())) {
                                queue.addLast(new Pending(callee, pending.depth + 1));
                            }
                        }
                    }

                    if (direction.equals("callers") || direction.equals("both")) {
                        for (Function caller : sorted(current.getCallingFunctions(monitor))) {
                            boolean known = discovered.contains(caller.getEntryPoint());
                            if (!known && discovered.size() >= maxNodes) continue;
                            writeEdge(edges, canonicalSeed, "caller", pending.depth + 1, caller, current);
                            if (discovered.add(caller.getEntryPoint())) {
                                queue.addLast(new Pending(caller, pending.depth + 1));
                            }
                        }
                    }
                }
            }
        }

        println("Exported bounded call graph to " + outputDirectory.getAbsolutePath());
    }
}
