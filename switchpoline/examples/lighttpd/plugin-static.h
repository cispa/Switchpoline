PLUGIN_INIT(mod_rewrite) /* PCRE */
PLUGIN_INIT(mod_redirect) /* PCRE */
PLUGIN_INIT(mod_alias)

PLUGIN_INIT(mod_extforward)

PLUGIN_INIT(mod_access)
PLUGIN_INIT(mod_auth) /* CRYPT LDAP LBER */
PLUGIN_INIT(mod_authn_file)

PLUGIN_INIT(mod_setenv)

#ifdef HAVE_LUA
// TODO: check
// PLUGIN_INIT(mod_magnet) /* LUA */
#endif
PLUGIN_INIT(mod_flv_streaming)

PLUGIN_INIT(mod_indexfile)
PLUGIN_INIT(mod_userdir)
PLUGIN_INIT(mod_dirlisting)

PLUGIN_INIT(mod_status)

PLUGIN_INIT(mod_simple_vhost)

PLUGIN_INIT(mod_secdownload)

PLUGIN_INIT(mod_cgi)
PLUGIN_INIT(mod_fastcgi)
PLUGIN_INIT(mod_scgi)
PLUGIN_INIT(mod_ssi) /* PCRE */
PLUGIN_INIT(mod_deflate)
PLUGIN_INIT(mod_proxy)

/* staticfile must come after cgi/ssi/et al. */
PLUGIN_INIT(mod_staticfile)

/* post-processing modules */
PLUGIN_INIT(mod_evasive)
PLUGIN_INIT(mod_usertrack)
PLUGIN_INIT(mod_expire)
PLUGIN_INIT(mod_accesslog)
