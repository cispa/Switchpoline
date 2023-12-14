#ifndef LLVM_CUSTOMSETTINGS_H
#define LLVM_CUSTOMSETTINGS_H

#include <string>

namespace typegraph {

    struct CustomSettings {
        CustomSettings();

        void setOutput(const std::string &Filename);

        bool enabled;
        bool novt_enabled;
        bool noic_enabled;
        bool dump_llvm;

        int enforce_id_bitwidth;
        unsigned long enforce_min_id;
        int enforce_dispatcher_limit; // min number of calls to a target set => calls get replaced by single dispatcher
        bool enforce_debug;
        bool enforce_jit;
        bool enforce_jit_link; // always link the JIT, even if it might come from the libc
        bool dynamic_linking;
        bool novt_jit_all;
        bool noic_jit_all;
        bool noic_all_id_func;
        bool protect_spectre_v1;

        bool export_novt;
        bool export_noic;

        bool no_lto; // compatibility flag to disable LTO in a single file
        bool musl_ldso_mode; // flag to build musl again - with all symbols hidden

        // TODO select base CFI policy
        // TODO libc/libcxx linking infos
        bool link_with_libc;
        bool link_with_compilerrt;

        bool indirect_call_remaining_warn;
        bool indirect_call_remaining_error;

        // Options from lld command line
        bool lld_is_shared = false;
        std::string output_filename;
    };
    extern CustomSettings Settings;

} // namespace typegraph

#endif //LLVM_CUSTOMSETTINGS_H
