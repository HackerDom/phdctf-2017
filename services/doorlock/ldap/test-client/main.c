#include <stdio.h>
#include <lber.h>
#include <ldap.h>

// based on: https://www.ldap.ncsu.edu/examples/gethomedir.c.txt

LDAP *ld;
int result;
int auth_method      = LDAP_AUTH_SIMPLE;
int desired_version  = LDAP_VERSION3;
char ldap_host[]     = "localhost";
char root_dn[]       = "cn=admin,dc=iot,dc=phdays,dc=com";
char root_pw[]       = "XfhC57uwby3plBWD";

int main(int argc, char**argv) {
  int ret;

  if (argc != 3) {
    printf("args: lockId cn\n");
    return 1;
  }

  if (ldap_initialize(&ld, "ldap://127.0.0.1:389")) {
    printf("ldap_initialize failed\n");
    return 1;
  }
  printf("ldap_initialize ok\n");

  int protocol_version = LDAP_VERSION3;
  ret = ldap_set_option(ld, LDAP_OPT_PROTOCOL_VERSION, &protocol_version);
  if (result != LDAP_SUCCESS) {
      printf("ldap_set_option: %d - %s\n", ret, ldap_err2string(ret));
      return 1;
  }
  printf("ldap_set_option ok\n");

  /* do an anonymous bind to the server, _s is for synchronous */
	ret = ldap_simple_bind_s(ld, NULL, NULL);
	if (ret != LDAP_SUCCESS) {
		printf("ldap_simple_bind_s failed: %d - %s\n", ret, ldap_err2string(ret));
		return 1;
	}
  printf("ldap_simple_bind_s ok\n");

  char query[512];
  sprintf(query, "(&(lockId=%s)(cn=%s))", argv[1], argv[2]);
  printf("LDAP query: '%s'\n", query);
  char *searchattrs[2];
  searchattrs[0] = "cardTag";
  searchattrs[1] = NULL;

  LDAPMessage *results, *entry;
  BerElement *ber;
  char **vals, *attr;

  ret = ldap_search_s(ld, "cn=locks,dc=iot,dc=phdays,dc=com",	LDAP_SCOPE_SUBTREE, query, searchattrs, 0, &results);
	if (ret != LDAP_SUCCESS) {
		printf("ldap_search_s failed: %d - %s\n", ret, ldap_err2string(ret));
		return 1;
	}
  printf("ldap_search_s ok\n");

	entry = ldap_first_entry(ld, results);
	if (!entry) {
		printf("ldap_first_entry: no results\n");
		return 1;
	}
  printf("ldap_first_entry ok\n");

	attr = ldap_first_attribute(ld, entry, &ber);
	if (!attr) {
		printf("ldap_first_attribute: no attributes.\n");
		return 1;
	}
  printf("ldap_first_attribute ok\n");

	vals = ldap_get_values(ld, entry, attr);
	if (!vals) {
		printf("ldap_get_values: no values.\n");
		return 1;
	}
  printf("ldap_get_values ok\n");

	printf("LDAP query result: %s\n", vals[0]);

  ldap_value_free(vals);
	ldap_memfree(attr);
	if (ber != NULL) {
		ber_free(ber, 0);
	}
	ldap_msgfree(entry);
	ldap_unbind(ld);
  return 0;
}
