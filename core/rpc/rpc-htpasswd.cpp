#include <string.h>
#include <limits.h> //PATH_MAX
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <iostream>

#include "agohttp/mongoose.h"

#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif

/* Copy of cs_md5 from mongoose.c which is not exposed */
void ago_cs_md5(char buf[33], ...) {
    unsigned char hash[16];
    const uint8_t *msgs[20], *p;
    size_t msg_lens[20];
    size_t num_msgs = 0;
    va_list ap;

    va_start(ap, buf);
    while ((p = va_arg(ap, const unsigned char *) ) != NULL) {
        msgs[num_msgs] = p;
        msg_lens[num_msgs] = va_arg(ap, size_t);
        num_msgs++;
    }
    va_end(ap);

    mg_hash_md5_v(num_msgs, msgs, msg_lens, hash);
    cs_to_hex(buf, hash, sizeof(hash));
}



//code from https://github.com/cesanta/mongoose/blob/master/examples/server.c#L339
int modify_passwords_file(const char *fname, const char *domain,
        const char *user, const char *pass) {
    int found;
    char line[512], u[512], d[512], ha1[33], tmp[PATH_MAX];
    FILE *fp, *fp2;

    found = 0;
    fp = fp2 = NULL;

    // Regard empty password as no password - remove user record.
    if (pass != NULL && pass[0] == '\0') {
        pass = NULL;
    }

    (void) snprintf(tmp, sizeof(tmp), "%s.tmp", fname);

    // Create the file if does not exist
    if ((fp = fopen(fname, "a+")) != NULL) {
        fclose(fp);
    }

    // Open the given file and temporary file
    if ((fp = fopen(fname, "r")) == NULL) {
        return 0;
    } else if ((fp2 = fopen(tmp, "w+")) == NULL) {
        fclose(fp);
        return 0;
    }

    // Copy the stuff to temporary file
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (sscanf(line, "%[^:]:%[^:]:%*s", u, d) != 2) {
            continue;
        }

        if (!strcmp(u, user) && !strcmp(d, domain)) {
            found++;
            if (pass != NULL) {
                ago_cs_md5(ha1, user, strlen(user), ":", 1, domain, strlen(domain), ":", 1, pass, strlen(pass), NULL);
                fprintf(fp2, "%s:%s:%s\n", user, domain, ha1);
            }
        } else {
            fprintf(fp2, "%s", line);
        }
    }

    // If new user, just add it
    if (!found && pass != NULL) {
        ago_cs_md5(ha1, user, strlen(user), ":", 1, domain, strlen(domain), ":", 1, pass, strlen(pass), NULL);
        fprintf(fp2, "%s:%s:%s\n", user, domain, ha1);
    }

    // Close files
    fclose(fp);
    fclose(fp2);

    // Put the temp file in place of real file
    remove(fname);
    rename(tmp, fname);

    return 1;
}

int main(int argc, char **argv) {
    if (argc != 5) {
        std::cout << "Usage: " << argv[0] << " <filename> <domainname> <username> <password>" << std::endl;
        exit(-1);
    }

    modify_passwords_file(argv[1], argv[2], argv[3], argv[4]);
}
