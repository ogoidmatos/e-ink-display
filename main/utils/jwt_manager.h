#ifndef JWT_MANAGER_H
#define JWT_MANAGER_H

#include <stdio.h>

char* createGCPJWT(const char* email, uint8_t* privateKey, size_t privateKeySize);

#endif /* JWT_MANAGER_H */