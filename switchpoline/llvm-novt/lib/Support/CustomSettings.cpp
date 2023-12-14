#include "llvm/Support/CustomSettings.h"
#include <cstdlib>
#include <string>

namespace typegraph {

    namespace {
        bool env_to_bool(const char *env, bool def = false) {
          auto *cp = getenv(env);
          if (cp) {
            std::string s(cp);
            if (s == "1" || s == "on" || s == "true")
              return true;
            if (s == "0" || s == "off" || s == "false")
              return false;
          }
          return def;
        }

        int env_to_int(const char *env, int def = 0) {
          auto *cp = getenv(env);
          if (cp) {
            return std::stoi(std::string(cp));
          }
          return def;
        }

        int env_to_ulong(const char *env, unsigned long def = 0) {
          auto *cp = getenv(env);
          if (cp) {
            return std::stoul(std::string(cp));
          }
          return def;
        }
    } // namespace

    CustomSettings::CustomSettings() {
      enabled = env_to_bool("TG_ENABLED", true);
      noic_enabled = env_to_bool("NOIC_ENABLED", true);
      novt_enabled = env_to_bool("NOVT_ENABLED", true);
      dump_llvm = env_to_bool("TG_DUMP_LLVM", true);
      enforce_id_bitwidth = env_to_int("TG_ENFORCE_ID_BITWIDTH", 31);
      enforce_min_id = env_to_ulong("TG_ENFORCE_MIN_ID", 2);
      enforce_dispatcher_limit = env_to_int("TG_ENFORCE_DISPATCHER_LIMIT", 5);
      enforce_debug = env_to_bool("TG_ENFORCE_DEBUG");
      enforce_jit = env_to_bool("TG_ENFORCE_JIT");
      enforce_jit_link = env_to_bool("TG_ENFORCE_JIT_LINK");
      dynamic_linking = env_to_bool("TG_DYNAMIC_LINKING");
      novt_jit_all = env_to_bool("TG_NOVT_JIT_ALL");
      noic_jit_all = env_to_bool("TG_NOIC_JIT_ALL");
      noic_all_id_func = noic_jit_all || env_to_bool("TG_NOIC_ALL_ID_FUNC");
      link_with_libc = env_to_bool("TG_LINK_WITH_LIBC");
      indirect_call_remaining_warn = env_to_bool("TG_INDIRECT_CALLS_WARN", true);
      indirect_call_remaining_error = env_to_bool("TG_INDIRECT_CALLS_ERROR");
      protect_spectre_v1 = env_to_bool("TG_PROTECT_SPECTRE_V1", false);
      no_lto = env_to_bool("TG_NO_LTO");
      musl_ldso_mode = env_to_bool("TG_MUSL_LDSO_MODE");
      export_novt = env_to_bool("TG_EXPORT_NOVT", true);
      export_noic = env_to_bool("TG_EXPORT_NOIC", true);
    }

    void CustomSettings::setOutput(const std::string &Filename) {
      // TODO remove pathname
      output_filename = Filename;
    }

    CustomSettings Settings;

} // namespace typegraph