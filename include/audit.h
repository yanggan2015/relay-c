#ifndef RELAY_AUDIT_H
#define RELAY_AUDIT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Append one access line to logs/access_YYYYMMDD.log (creates logs/ if needed). */
void relay_audit(const char *ip, const char *method, const char *path,
                 const char *qs, int status, const char *note);

#ifdef __cplusplus
}
#endif

#endif
