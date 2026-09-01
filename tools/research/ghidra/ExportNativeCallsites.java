// Exports direct and computed call instructions from explicit seed functions.
// @category ERNativeUI

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.Set;
import java.util.TreeSet;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.Symbol;

public class ExportNativeCallsites extends GhidraScript {
    private static final class VirtualGuess {
        boolean probable;
        String baseRegister = "";
        Long byteOffset;
        Long slot;
        String reason = "";
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

    private static long nonNegativeLong(String value, String name) {
        String normalized = value.trim().toLowerCase(Locale.ROOT);
        long parsed = normalized.startsWith("0x")
            ? Long.parseUnsignedLong(normalized.substring(2), 16)
            : Long.parseLong(normalized);
        if (parsed < 0) throw new IllegalArgumentException(name + " must not be negative");
        return parsed;
    }

    private static long parseRva(String value) {
        String normalized = value.trim().toLowerCase(Locale.ROOT);
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

    private String bytes(Instruction instruction) {
        try {
            byte[] values = instruction.getBytes();
            StringBuilder result = new StringBuilder();
            for (byte value : values) result.append(String.format("%02X", value));
            return result.toString();
        }
        catch (Exception exception) {
            return "";
        }
    }

    private static boolean excludedVirtualBase(Register register) {
        String name = register.getName().toUpperCase(Locale.ROOT);
        return name.equals("RIP") || name.equals("EIP") || name.equals("IP") ||
            name.equals("RSP") || name.equals("ESP") || name.equals("SP") ||
            name.equals("RBP") || name.equals("EBP") || name.equals("BP");
    }

    private VirtualGuess probableVirtualCall(Instruction instruction, long maxVirtualOffset) {
        VirtualGuess result = new VirtualGuess();
        if (!instruction.getFlowType().isComputed() || instruction.getNumOperands() == 0) {
            result.reason = "not_a_computed_call";
            return result;
        }

        String representation = instruction.getDefaultOperandRepresentation(0);
        if (!representation.contains("[") || !representation.contains("]")) {
            result.reason = "computed_call_without_memory_operand";
            return result;
        }
        if (representation.contains("*")) {
            result.reason = "indexed_memory_operand";
            return result;
        }

        List<Register> registers = new ArrayList<>();
        List<Scalar> scalars = new ArrayList<>();
        for (Object object : instruction.getOpObjects(0)) {
            if (object instanceof Register) registers.add((Register)object);
            else if (object instanceof Scalar) scalars.add((Scalar)object);
        }
        if (registers.size() != 1 || excludedVirtualBase(registers.get(0))) {
            result.reason = "memory_operand_does_not_have_one_object_base_register";
            return result;
        }

        long offset = 0;
        if (!scalars.isEmpty()) {
            boolean found = false;
            for (Scalar scalar : scalars) {
                long candidate = scalar.getSignedValue();
                if (candidate >= 0 && candidate <= maxVirtualOffset &&
                    candidate % pointerSize == 0) {
                    offset = candidate;
                    found = true;
                }
            }
            if (!found) {
                result.reason = "no_aligned_bounded_displacement";
                return result;
            }
        }

        result.probable = true;
        result.baseRegister = registers.get(0).getName();
        result.byteOffset = offset;
        result.slot = offset / pointerSize;
        result.reason = "computed_memory_call_through_one_object_base_register";
        return result;
    }

    private String callKind(Instruction instruction) {
        if (!instruction.getFlowType().isComputed()) return "direct";
        String representation = instruction.getNumOperands() == 0 ? "" :
            instruction.getDefaultOperandRepresentation(0);
        if (representation.contains("[") && representation.contains("]")) {
            return "indirect_memory";
        }
        for (Object object : instruction.getNumOperands() == 0 ? new Object[0] :
                instruction.getOpObjects(0)) {
            if (object instanceof Register) return "indirect_register";
        }
        return "indirect_computed";
    }

    private String symbolName(Address address) {
        Symbol symbol = currentProgram.getSymbolTable().getPrimarySymbol(address);
        return symbol == null ? "" : symbol.getName(true);
    }

    private String join(Set<String> values) {
        return String.join(";", values);
    }

    private void writeCall(BufferedWriter writer, String requestedSeed, Function function,
            Instruction instruction, long maxVirtualOffset) throws Exception {
        Set<String> targetAddresses = new TreeSet<>();
        Set<String> targetRvas = new TreeSet<>();
        Set<String> targetSymbols = new TreeSet<>();
        Set<String> targetFunctions = new TreeSet<>();

        for (Address target : instruction.getFlows()) {
            targetAddresses.add(target.toString());
            String targetRva = rva(target);
            if (!targetRva.isEmpty()) targetRvas.add(targetRva);
            String symbol = symbolName(target);
            if (!symbol.isEmpty()) targetSymbols.add(symbol);
            Function targetFunction = currentProgram.getFunctionManager().getFunctionAt(target);
            if (targetFunction != null) {
                targetFunctions.add(rva(targetFunction.getEntryPoint()) + ":" +
                    targetFunction.getName(true));
            }
        }
        for (Reference reference : instruction.getReferencesFrom()) {
            if (!reference.getReferenceType().isCall()) continue;
            Address target = reference.getToAddress();
            targetAddresses.add(target.toString());
            String targetRva = rva(target);
            if (!targetRva.isEmpty()) targetRvas.add(targetRva);
            String symbol = symbolName(target);
            if (!symbol.isEmpty()) targetSymbols.add(symbol);
            Function targetFunction = currentProgram.getFunctionManager().getFunctionAt(target);
            if (targetFunction != null) {
                targetFunctions.add(rva(targetFunction.getEntryPoint()) + ":" +
                    targetFunction.getName(true));
            }
        }

        VirtualGuess guess = probableVirtualCall(instruction, maxVirtualOffset);
        writer.write(String.format(
            "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s%n",
            csv(requestedSeed), rva(function.getEntryPoint()), csv(function.getName(true)),
            rva(instruction.getAddress()), csv(bytes(instruction)), csv(instruction.toString()),
            csv(instruction.getFlowType().toString()), csv(callKind(instruction)),
            csv(join(targetAddresses)), csv(join(targetRvas)), csv(join(targetSymbols)),
            csv(join(targetFunctions)), guess.probable, csv(guess.baseRegister),
            guess.byteOffset == null ? "" : String.format("0x%X", guess.byteOffset),
            guess.slot == null ? "" : guess.slot.toString(), csv(guess.reason),
            instruction.getDelaySlotDepth()));
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 5) {
            throw new IllegalArgumentException(
                "Expected: output_directory max_instructions_per_seed max_calls_per_seed " +
                "max_virtual_byte_offset seed_rva [seed_rva ...]");
        }

        File outputDirectory = new File(args[0]);
        outputDirectory.mkdirs();
        int maxInstructions = positiveInt(args[1], "max_instructions_per_seed");
        int maxCalls = positiveInt(args[2], "max_calls_per_seed");
        long maxVirtualOffset = nonNegativeLong(args[3], "max_virtual_byte_offset");
        imageBase = currentProgram.getImageBase();
        pointerSize = currentProgram.getDefaultPointerSize();

        File callsFile = new File(outputDirectory, "calls.csv");
        File seedsFile = new File(outputDirectory, "seeds.csv");
        try (BufferedWriter calls = new BufferedWriter(new FileWriter(callsFile));
             BufferedWriter seeds = new BufferedWriter(new FileWriter(seedsFile))) {
            calls.write("requested_seed,function_rva,function_name,instruction_rva,bytes," +
                "instruction,flow_type,call_kind,target_addresses,target_rvas,target_symbols," +
                "target_functions,probable_virtual_call,object_base_register," +
                "probable_vtable_byte_offset,probable_vtable_slot,classification_reason," +
                "delay_slot_depth\n");
            seeds.write("requested_seed,function_rva,function_name,instructions_scanned," +
                "instructions_truncated,calls_found,calls_retained,calls_truncated,status\n");

            for (int argument = 4; argument < args.length && !monitor.isCancelled(); ++argument) {
                String requestedSeed = args[argument];
                Address seedAddress = imageBase.add(parseRva(requestedSeed));
                Function function = currentProgram.getFunctionManager().getFunctionContaining(seedAddress);
                if (function == null) {
                    seeds.write(String.format("%s,,,,false,0,0,false,%s%n", csv(requestedSeed),
                        csv("function_not_found")));
                    continue;
                }

                int instructionsScanned = 0;
                int callsFound = 0;
                int callsRetained = 0;
                InstructionIterator iterator = currentProgram.getListing()
                    .getInstructions(function.getBody(), true);
                while (iterator.hasNext() && instructionsScanned < maxInstructions &&
                        !monitor.isCancelled()) {
                    Instruction instruction = iterator.next();
                    ++instructionsScanned;
                    if (!instruction.getFlowType().isCall()) continue;
                    ++callsFound;
                    if (callsRetained >= maxCalls) continue;
                    writeCall(calls, requestedSeed, function, instruction, maxVirtualOffset);
                    ++callsRetained;
                }
                boolean instructionTruncated = iterator.hasNext();
                seeds.write(String.format("%s,%s,%s,%d,%s,%d,%d,%s,%s%n",
                    csv(requestedSeed), rva(function.getEntryPoint()), csv(function.getName(true)),
                    instructionsScanned, instructionTruncated, callsFound, callsRetained,
                    callsFound > callsRetained, csv(monitor.isCancelled() ? "cancelled" : "ok")));
            }
        }

        println("Exported bounded native callsites to " + outputDirectory.getAbsolutePath());
    }
}
