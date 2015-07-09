/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/* From: https://svn.apache.org/repos/asf/apr/apr/trunk/strings/apr_strtok.c */
/* with changes to work in Ghostscript */

#include "string_.h"
#include "gsstrtok.h"

#define APR_DECLARE(X) static X

APR_DECLARE(char *) apr_strtok(char *str, const char *sep, char **last)
{
    char *token;

    if (!str)           /* subsequent call */
        str = *last;    /* start where we left off */

    /* skip characters in sep (will terminate at '\0') */
    while (*str && strchr(sep, *str))
        ++str;

    if (!*str)          /* no more tokens */
        return NULL;

    token = str;

    /* skip valid token characters to terminate token and
     * prepare for the next call (will terminate at '\0)
     */
    *last = token + 1;
    while (**last && !strchr(sep, **last))
        ++*last;

    if (**last) {
        **last = '\0';
        ++*last;
    }

    return token;
}

char * gs_strtok(char *str, const char *sep, char **last)
{
    return apr_strtok(str, sep, last);
}
