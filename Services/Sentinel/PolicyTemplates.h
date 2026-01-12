/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/HashMap.h>
#include <AK/String.h>
#include <AK/Vector.h>

namespace Sentinel {

struct PolicyTemplateDefinition {
    String name;
    String description;
    String category;
    String template_json;
    HashMap<String, String> variables;
};

class PolicyTemplates {
public:
    static Vector<PolicyTemplateDefinition> get_builtin_templates();
};

}
