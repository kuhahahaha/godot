extends RefCounted

const FIELDS = ["schema", "core_stamp", "compiler_route", "compiler_stamp", "platform", "architecture", "target_kind"]

# Engine emits schema as INT. JSON.parse_string represents JSON numbers as
# FLOAT. Compare the closed identity field-by-field without coercing stamps or
# accepting nonintegral/foreign schema values.
static func matches(actual: Variant, expected: Variant) -> bool:
    if typeof(actual) != TYPE_DICTIONARY or typeof(expected) != TYPE_DICTIONARY:
        return false
    if actual.size() != FIELDS.size() or expected.size() != FIELDS.size():
        return false
    for key in actual.keys():
        if typeof(key) != TYPE_STRING or not FIELDS.has(key):
            return false
    for key in expected.keys():
        if typeof(key) != TYPE_STRING or not FIELDS.has(key):
            return false
    for field in FIELDS:
        if not actual.has(field) or not expected.has(field):
            return false
    if typeof(actual["schema"]) != TYPE_INT or actual["schema"] != 1:
        return false
    if typeof(expected["schema"]) != TYPE_INT and typeof(expected["schema"]) != TYPE_FLOAT:
        return false
    if expected["schema"] != 1:
        return false
    for field in FIELDS:
        if field == "schema":
            continue
        if typeof(actual[field]) != TYPE_STRING or typeof(expected[field]) != TYPE_STRING:
            return false
        if actual[field] != expected[field]:
            return false
    return true
