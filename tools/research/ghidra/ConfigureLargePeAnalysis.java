// Configure a conservative auto-analysis profile for very large optimized x86-64 PE files.
// Intended for analyzeHeadless -preScript, before the automatic analysis pass starts.
//@category ERNativeUI.Research

import java.util.LinkedHashMap;
import java.util.Locale;
import java.util.Map;
import java.util.Objects;

import ghidra.app.script.GhidraScript;

public class ConfigureLargePeAnalysis extends GhidraScript {
    private static final String DISCOVERED_NORETURN =
        "Non-Returning Functions - Discovered";
    private static final String DISCOVERED_NORETURN_REPAIR =
        "Non-Returning Functions - Discovered.Repair Flow Damage";
    private static final String AGGRESSIVE_INSTRUCTION_FINDER =
        "Aggressive Instruction Finder";
    private static final String FUNCTION_STARTS_IN_DATA =
        "Function Start Search.Search Data Blocks";

    @Override
    protected void run() throws Exception {
        requireSupportedProgram();

        Map<String, String> available = getCurrentAnalysisOptionsAndValues(currentProgram);
        LinkedHashMap<String, String> profile = new LinkedHashMap<>();

        // This heuristic can cascade when an executable region contains invalid or
        // intentionally confusing flows. It marks inferred targets no-return, rewrites
        // caller flow, repairs function bodies, and (by default) clears/re-disassembles
        // every fall-through it considers damaged. Known no-return functions are handled
        // by the separate "Non-Returning Functions - Known" analyzer, which stays enabled.
        profile.put(DISCOVERED_NORETURN, "false");
        profile.put(DISCOVERED_NORETURN_REPAIR, "false");

        // These are disabled by Ghidra's x86-64 PE defaults. Pin them off so the profile
        // cannot silently become a scan of arbitrary undefined/data bytes.
        profile.put(AGGRESSIVE_INSTRUCTION_FINDER, "false");
        profile.put(FUNCTION_STARTS_IN_DATA, "false");

        requireOptions(available, profile);
        setAnalysisOptions(currentProgram, profile);
        verifyOptions(profile);
        verifyPreservedAnalyzers(available);

        println("Applied conservative large-PE analysis profile:");
        println("  disabled: " + DISCOVERED_NORETURN);
        println("  disabled: " + DISCOVERED_NORETURN_REPAIR);
        println("  disabled: " + AGGRESSIVE_INSTRUCTION_FINDER);
        println("  disabled: " + FUNCTION_STARTS_IN_DATA);
        println("  unchanged: PE exception handling, PE RTTI, Microsoft demangling, strings,");
        println("             references, executable-block function starts, and decompiler analysis");
    }

    private void requireSupportedProgram() {
        String format = currentProgram.getExecutableFormat();
        String processor = currentProgram.getLanguage().getProcessor().toString();
        if (format == null ||
            !format.toLowerCase(Locale.ROOT).contains("portable executable") ||
            !"x86".equals(processor) || currentProgram.getDefaultPointerSize() != 8) {
            throw new IllegalStateException(
                "ConfigureLargePeAnalysis supports only x86-64 Portable Executable programs; " +
                "got format=" + format + ", processor=" + processor +
                ", pointerSize=" + currentProgram.getDefaultPointerSize());
        }
    }

    private void requireOptions(Map<String, String> available, Map<String, String> requested) {
        for (String name : requested.keySet()) {
            if (!available.containsKey(name)) {
                throw new IllegalStateException(
                    "Required Ghidra analysis option is unavailable: " + name);
            }
        }
    }

    private void verifyOptions(Map<String, String> expected) {
        Map<String, String> actual = getCurrentAnalysisOptionsAndValues(currentProgram);
        for (Map.Entry<String, String> entry : expected.entrySet()) {
            String value = actual.get(entry.getKey());
            if (!entry.getValue().equalsIgnoreCase(value)) {
                throw new IllegalStateException(
                    "Failed to set analysis option " + entry.getKey() + ": expected " +
                    entry.getValue() + ", got " + value);
            }
        }
    }

    private void verifyPreservedAnalyzers(Map<String, String> before) {
        String[] preserved = {
            "ASCII Strings",
            "Call Convention ID",
            "Call-Fixup Installer",
            "Data Reference",
            "Decompiler Parameter ID",
            "Decompiler Switch Analysis",
            "Demangler Microsoft",
            "Disassemble Entry Points",
            "Function ID",
            "Function Start Search",
            "Non-Returning Functions - Known",
            "Reference",
            "Shared Return Calls",
            "Stack",
            "Subroutine References",
            "Windows x86 PE Exception Handling",
            "Windows x86 PE RTTI Analyzer",
            "x86 Constant Reference Analyzer"
        };

        Map<String, String> actual = getCurrentAnalysisOptionsAndValues(currentProgram);
        for (String name : preserved) {
            if (!before.containsKey(name) || !actual.containsKey(name)) {
                throw new IllegalStateException(
                    "Required preserved Ghidra analysis option is unavailable: " + name);
            }
            if (!Objects.equals(before.get(name), actual.get(name))) {
                throw new IllegalStateException(
                    "Conservative profile unexpectedly changed analyzer " + name + ": " +
                    before.get(name) + " -> " + actual.get(name));
            }
        }
    }
}
