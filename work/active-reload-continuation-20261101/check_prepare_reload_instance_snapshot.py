#!/usr/bin/env python3
"""Check that reload state capture does not iterate a callback-mutable set."""

from pathlib import Path
import re
import sys


SOURCE = Path(__file__).resolve().parents[2] / "core/extension/gdextension.cpp"


def extract_function(source: str, signature: str) -> str:
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1 : index]
    raise AssertionError(f"unterminated function: {signature}")


def main() -> int:
    body = extract_function(SOURCE.read_text(), "void GDExtension::prepare_reload()")
    mutable_iteration = re.search(r"for\s*\(\s*const ObjectID\s*&\s*obj_id\s*:\s*E\.value\.instances\s*\)", body)
    snapshot = re.search(r"Vector<ObjectID>\s+\w+\s*;[\s\S]{0,400}E\.value\.instances", body)
    reacquire_after_property_list = re.search(
        r"get_property_list\(&prop_list\);[\s\S]{0,180}ObjectDB::get_instance\(obj_id\)", body
    )
    reacquire_after_get = re.search(
        r"Variant value = obj->get\(P\.name\);[\s\S]{0,120}ObjectDB::get_instance\(obj_id\)", body
    )
    reacquire_before_get = re.search(
        r"obj = ObjectDB::get_instance\(obj_id\);\s*if \(!obj\) \{\s*instance_alive = false;\s*break;\s*\}\s*Variant value = obj->get\(P\.name\);",
        body,
    )
    reacquire_after_default = re.search(
        r"class_get_default_property_value\(class_name, P\.name\);\s*obj = ObjectDB::get_instance\(obj_id\);\s*if \(!obj\)",
        body,
    )
    owns_class_name = "StringName class_name = obj->get_class_name();" in body
    if mutable_iteration and not snapshot:
        print("RED: prepare_reload iterates E.value.instances directly while get_property_list/get can re-enter and remove instances")
        return 1
    if snapshot and not reacquire_after_property_list:
        print("RED: prepare_reload snapshots IDs but keeps obj across get_property_list callback")
        return 1
    if snapshot and not reacquire_after_get:
        print("RED: prepare_reload snapshots IDs but keeps obj across get callback")
        return 1
    if snapshot and not reacquire_before_get:
        print("RED: prepare_reload reuses obj after default lookup or temporary Variant destruction")
        return 1
    if snapshot and not (owns_class_name and reacquire_after_default):
        print("RED: prepare_reload retains object-owned state across default-value callbacks")
        return 1
    if snapshot:
        print("GREEN: prepare_reload snapshots instance IDs and reacquires objects across property/default-value callbacks")
        return 0
    print("UNCLASSIFIED: inspect prepare_reload iteration shape")
    return 2


if __name__ == "__main__":
    sys.exit(main())
