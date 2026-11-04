extends SceneTree

const SelectionIdentity = preload("res://selection_identity.gd")
var checks = 0
var failures = 0

func check(value: bool, label: String) -> void:
    checks += 1
    if not value:
        failures += 1
        push_error("SELECTION_IDENTITY_SELF_TEST_FAIL " + label)

func _initialize():
    var actual = Engine.get_version_info().get("controlled_build_identity", {})
    check(typeof(actual) == TYPE_DICTIONARY and actual.size() == 7, "actual Engine closed identity available")
    if failures != 0:
        quit(1)
        return
    var expected = JSON.parse_string(JSON.stringify(actual))
    check(typeof(actual["schema"]) == TYPE_INT, "actual Engine schema INT")
    check(typeof(expected["schema"]) == TYPE_FLOAT, "JSON schema FLOAT")
    check(SelectionIdentity.matches(actual, actual), "correct expected INT schema")
    check(SelectionIdentity.matches(actual, expected), "correct JSON FLOAT schema")
    for schema in [0, 2, 1.5, "1", true, null]:
        var wrong = expected.duplicate(true)
        wrong["schema"] = schema
        check(not SelectionIdentity.matches(actual, wrong), "reject expected schema " + str(schema))
    for schema in [1.0, 0, 2, "1", true, null]:
        var wrong_actual = actual.duplicate(true)
        wrong_actual["schema"] = schema
        check(not SelectionIdentity.matches(wrong_actual, expected), "reject Engine schema " + str(schema))
    for field in SelectionIdentity.FIELDS:
        var missing_expected = expected.duplicate(true)
        missing_expected.erase(field)
        check(not SelectionIdentity.matches(actual, missing_expected), "reject missing expected " + field)
        var missing_actual = actual.duplicate(true)
        missing_actual.erase(field)
        check(not SelectionIdentity.matches(missing_actual, expected), "reject missing Engine " + field)
        if field != "schema":
            var wrong_expected_type = expected.duplicate(true)
            wrong_expected_type[field] = 1
            check(not SelectionIdentity.matches(actual, wrong_expected_type), "reject expected type " + field)
            var wrong_actual_type = actual.duplicate(true)
            wrong_actual_type[field] = 1
            check(not SelectionIdentity.matches(wrong_actual_type, expected), "reject Engine type " + field)
            var changed = expected.duplicate(true)
            changed[field] = str(changed[field]) + "changed"
            check(not SelectionIdentity.matches(actual, changed), "reject changed " + field)
    var unknown_expected = expected.duplicate(true)
    unknown_expected["unknown"] = "extra"
    check(not SelectionIdentity.matches(actual, unknown_expected), "reject unknown expected key")
    var unknown_actual = actual.duplicate(true)
    unknown_actual["unknown"] = "extra"
    check(not SelectionIdentity.matches(unknown_actual, expected), "reject unknown Engine key")
    var replacement = expected.duplicate(true)
    replacement.erase("core_stamp")
    replacement["unknown"] = actual["core_stamp"]
    check(not SelectionIdentity.matches(actual, replacement), "reject unknown replacing missing same size")
    check(not SelectionIdentity.matches([], expected), "reject non-Dictionary Engine")
    check(not SelectionIdentity.matches(actual, []), "reject non-Dictionary expected")
    print("SELECTION_IDENTITY_SELF_TEST checks=", checks, " failures=", failures, " actual_schema_type=", typeof(actual["schema"]), " expected_json_schema_type=", typeof(expected["schema"]))
    quit(0 if failures == 0 else 1)
