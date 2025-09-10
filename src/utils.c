#include "kpm/kpm.h"
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void KPM_FreeInstallTarget(struct InstallTarget* target)
{
    free(target->id);
    free(target->repository);
    target->id = NULL;
    target->repository = NULL;
}

enum Internal_UserVersionComparison
{
    Internal_DTCMP_NONE,
    Internal_DTCMP_GT,
    Internal_DTCMP_GTEQ,
    Internal_DTCMP_EQ,
    Internal_DTCMP_LT,
    Internal_DTCMP_LTEQ
};

/**
 * @brief Resolve an install string such as "com.kindlemodding.repo/kpm>=5.1.2" into an InstallTarget object
 * 
 * @param installString The installation string
 * @param target A pointer to the InstallTarget
 * @return enum KPMResult 
 */
enum KPMResult KPM_ResolveInstallString(char* installString, struct InstallTarget* target)
{
    target->id = NULL;
    target->repository = NULL;
    target->dependency_type = KPM_DT_NONE;
    enum Internal_UserVersionComparison comparisonType = Internal_DTCMP_NONE;

    char* buffer = malloc(strlen(installString) + 1);
    int bufferIndex = 0;
    buffer[0] = '\0';

    for (size_t i=0; i < strlen(installString); i++)
    {
        if (installString[i] == '/')
        {
            if (target->repository != NULL)
            {
                goto error;
            }

            target->repository = strdup(buffer);
            buffer[0] = '\0';
            bufferIndex = 0;
            continue; // Skip '/'
        }

        if (installString[i] == '=' || installString[i] == '>' || installString[i] == '<')
        {
            if (target->id != NULL || comparisonType != Internal_DTCMP_NONE)
            {
                goto error;
            }

            target->id = strdup(buffer);

            if (installString[i] == '=')
            {
                i++;
                if (installString[i] == '=')
                {
                    comparisonType = Internal_DTCMP_EQ;
                }
                else {
                    goto error;
                }
            }
            else if (installString[i] == '>')
            {
                comparisonType = Internal_DTCMP_GT;
                if (installString[i+1] == '=')
                {
                    comparisonType = Internal_DTCMP_GTEQ;
                    i++;
                }
            }
            else if (installString[i] == '<')
            {
                comparisonType = Internal_DTCMP_LT;
                if (installString[i+1] == '=')
                {
                    comparisonType = Internal_DTCMP_LTEQ;
                    i++;
                }
            }

            buffer[0] = '\0';
            bufferIndex = 0;
            continue; // Skip character
        }

        buffer[bufferIndex++] = installString[i];
        buffer[bufferIndex] = '\0';
    }
    
    if (comparisonType == Internal_DTCMP_NONE)
    {
        target->id = strdup(buffer);
    }
    else {
        bool hasMajor = false;
        bool hasMinor = false;
        char* numberBuffer = malloc(strlen(buffer) + 1);
        numberBuffer[0] = '\0';
        bufferIndex = 0;

        for (size_t i=0; i < strlen(buffer); i++)
        {
            if (buffer[i] == '.')
            {
                if (!hasMajor)
                {
                    target->version.major = atoi(numberBuffer);
                    hasMajor = true;
                }
                else if (!hasMinor)
                {
                    target->version.minor = atoi(numberBuffer);
                    hasMinor = true;
                }

                numberBuffer[0] = '\0';
                bufferIndex = 0;
                continue; // To skip this char
            }

            numberBuffer[bufferIndex++] = buffer[i];
            numberBuffer[bufferIndex] = '\0';
        }

        target->version.patch = atoi(numberBuffer);
        free(numberBuffer);
    }

    free(buffer);

    switch (comparisonType)
    {
        case Internal_DTCMP_NONE:
            target->dependency_type = KPM_DT_NONE;
            break;
        case Internal_DTCMP_GT:
            target->dependency_type = KPM_DT_GREATER_THAN_OR_EQUAL_TO;
            target->version.patch++;
            break;
        case Internal_DTCMP_GTEQ:
            target->dependency_type = KPM_DT_GREATER_THAN_OR_EQUAL_TO;
            break;
        case Internal_DTCMP_EQ:
            target->dependency_type = KPM_DT_EQUAL_TO;
            break;
        case Internal_DTCMP_LT:
            target->dependency_type = KPM_DT_LESS_THAN_OR_EQUAL_TO;
            if (target->version.patch == 0)
            {
                target->version.patch = LONG_MAX;
                if (target->version.minor == 0)
                {
                    target->version.minor = LONG_MAX;
                    target->version.major--;
                }
                else {
                    target->version.minor--;
                }
            }
            else {
                target->version.patch--;
            }
            break;
        case Internal_DTCMP_LTEQ:
            target->dependency_type = KPM_DT_LESS_THAN_OR_EQUAL_TO;
            break;
    };

    return KPM_OK;

error:
    KPM_FreeInstallTarget(target);
    free(buffer);
    return KPM_GENERIC_ERROR;
}