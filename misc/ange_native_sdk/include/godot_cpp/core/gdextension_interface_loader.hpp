/**************************************************************************/
/*  gdextension_interface_loader.hpp                                      */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

// THIS FILE IS GENERATED. EDITS WILL BE LOST.

#pragma once

#include <gdextension_interface.h>
#include <godot_cpp/core/version.hpp>

namespace godot {

namespace gdextension_interface {

// Godot 4.1 or newer.
#if GODOT_VERSION_MINOR >= 1
#if !defined(DISABLE_DEPRECATED) || GODOT_VERSION_MINOR < 5
extern "C" GDExtensionInterfaceGetGodotVersion get_godot_version;
#endif
#if !defined(DISABLE_DEPRECATED) || GODOT_VERSION_MINOR < 6
extern "C" GDExtensionInterfaceMemAlloc mem_alloc;
#endif
#if !defined(DISABLE_DEPRECATED) || GODOT_VERSION_MINOR < 6
extern "C" GDExtensionInterfaceMemRealloc mem_realloc;
#endif
#if !defined(DISABLE_DEPRECATED) || GODOT_VERSION_MINOR < 6
extern "C" GDExtensionInterfaceMemFree mem_free;
#endif
extern "C" GDExtensionInterfacePrintError print_error;
extern "C" GDExtensionInterfacePrintErrorWithMessage print_error_with_message;
extern "C" GDExtensionInterfacePrintWarning print_warning;
extern "C" GDExtensionInterfacePrintWarningWithMessage print_warning_with_message;
extern "C" GDExtensionInterfacePrintScriptError print_script_error;
extern "C" GDExtensionInterfacePrintScriptErrorWithMessage print_script_error_with_message;
extern "C" GDExtensionInterfaceGetNativeStructSize get_native_struct_size;
extern "C" GDExtensionInterfaceVariantNewCopy variant_new_copy;
extern "C" GDExtensionInterfaceVariantNewNil variant_new_nil;
extern "C" GDExtensionInterfaceVariantDestroy variant_destroy;
extern "C" GDExtensionInterfaceVariantCall variant_call;
extern "C" GDExtensionInterfaceVariantCallStatic variant_call_static;
extern "C" GDExtensionInterfaceVariantEvaluate variant_evaluate;
extern "C" GDExtensionInterfaceVariantSet variant_set;
extern "C" GDExtensionInterfaceVariantSetNamed variant_set_named;
extern "C" GDExtensionInterfaceVariantSetKeyed variant_set_keyed;
extern "C" GDExtensionInterfaceVariantSetIndexed variant_set_indexed;
extern "C" GDExtensionInterfaceVariantGet variant_get;
extern "C" GDExtensionInterfaceVariantGetNamed variant_get_named;
extern "C" GDExtensionInterfaceVariantGetKeyed variant_get_keyed;
extern "C" GDExtensionInterfaceVariantGetIndexed variant_get_indexed;
extern "C" GDExtensionInterfaceVariantIterInit variant_iter_init;
extern "C" GDExtensionInterfaceVariantIterNext variant_iter_next;
extern "C" GDExtensionInterfaceVariantIterGet variant_iter_get;
extern "C" GDExtensionInterfaceVariantHash variant_hash;
extern "C" GDExtensionInterfaceVariantRecursiveHash variant_recursive_hash;
extern "C" GDExtensionInterfaceVariantHashCompare variant_hash_compare;
extern "C" GDExtensionInterfaceVariantBooleanize variant_booleanize;
extern "C" GDExtensionInterfaceVariantDuplicate variant_duplicate;
extern "C" GDExtensionInterfaceVariantStringify variant_stringify;
extern "C" GDExtensionInterfaceVariantGetType variant_get_type;
extern "C" GDExtensionInterfaceVariantHasMethod variant_has_method;
extern "C" GDExtensionInterfaceVariantHasMember variant_has_member;
extern "C" GDExtensionInterfaceVariantHasKey variant_has_key;
extern "C" GDExtensionInterfaceVariantGetTypeName variant_get_type_name;
extern "C" GDExtensionInterfaceVariantCanConvert variant_can_convert;
extern "C" GDExtensionInterfaceVariantCanConvertStrict variant_can_convert_strict;
extern "C" GDExtensionInterfaceGetVariantFromTypeConstructor get_variant_from_type_constructor;
extern "C" GDExtensionInterfaceGetVariantToTypeConstructor get_variant_to_type_constructor;
extern "C" GDExtensionInterfaceVariantGetPtrOperatorEvaluator variant_get_ptr_operator_evaluator;
extern "C" GDExtensionInterfaceVariantGetPtrBuiltinMethod variant_get_ptr_builtin_method;
extern "C" GDExtensionInterfaceVariantGetPtrConstructor variant_get_ptr_constructor;
extern "C" GDExtensionInterfaceVariantGetPtrDestructor variant_get_ptr_destructor;
extern "C" GDExtensionInterfaceVariantConstruct variant_construct;
extern "C" GDExtensionInterfaceVariantGetPtrSetter variant_get_ptr_setter;
extern "C" GDExtensionInterfaceVariantGetPtrGetter variant_get_ptr_getter;
extern "C" GDExtensionInterfaceVariantGetPtrIndexedSetter variant_get_ptr_indexed_setter;
extern "C" GDExtensionInterfaceVariantGetPtrIndexedGetter variant_get_ptr_indexed_getter;
extern "C" GDExtensionInterfaceVariantGetPtrKeyedSetter variant_get_ptr_keyed_setter;
extern "C" GDExtensionInterfaceVariantGetPtrKeyedGetter variant_get_ptr_keyed_getter;
extern "C" GDExtensionInterfaceVariantGetPtrKeyedChecker variant_get_ptr_keyed_checker;
extern "C" GDExtensionInterfaceVariantGetConstantValue variant_get_constant_value;
extern "C" GDExtensionInterfaceVariantGetPtrUtilityFunction variant_get_ptr_utility_function;
extern "C" GDExtensionInterfaceStringNewWithLatin1Chars string_new_with_latin1_chars;
extern "C" GDExtensionInterfaceStringNewWithUtf8Chars string_new_with_utf8_chars;
extern "C" GDExtensionInterfaceStringNewWithUtf16Chars string_new_with_utf16_chars;
extern "C" GDExtensionInterfaceStringNewWithUtf32Chars string_new_with_utf32_chars;
extern "C" GDExtensionInterfaceStringNewWithWideChars string_new_with_wide_chars;
extern "C" GDExtensionInterfaceStringNewWithLatin1CharsAndLen string_new_with_latin1_chars_and_len;
#if !defined(DISABLE_DEPRECATED) || GODOT_VERSION_MINOR < 3
extern "C" GDExtensionInterfaceStringNewWithUtf8CharsAndLen string_new_with_utf8_chars_and_len;
#endif
#if !defined(DISABLE_DEPRECATED) || GODOT_VERSION_MINOR < 3
extern "C" GDExtensionInterfaceStringNewWithUtf16CharsAndLen string_new_with_utf16_chars_and_len;
#endif
extern "C" GDExtensionInterfaceStringNewWithUtf32CharsAndLen string_new_with_utf32_chars_and_len;
extern "C" GDExtensionInterfaceStringNewWithWideCharsAndLen string_new_with_wide_chars_and_len;
extern "C" GDExtensionInterfaceStringToLatin1Chars string_to_latin1_chars;
extern "C" GDExtensionInterfaceStringToUtf8Chars string_to_utf8_chars;
extern "C" GDExtensionInterfaceStringToUtf16Chars string_to_utf16_chars;
extern "C" GDExtensionInterfaceStringToUtf32Chars string_to_utf32_chars;
extern "C" GDExtensionInterfaceStringToWideChars string_to_wide_chars;
extern "C" GDExtensionInterfaceStringOperatorIndex string_operator_index;
extern "C" GDExtensionInterfaceStringOperatorIndexConst string_operator_index_const;
extern "C" GDExtensionInterfaceStringOperatorPlusEqString string_operator_plus_eq_string;
extern "C" GDExtensionInterfaceStringOperatorPlusEqChar string_operator_plus_eq_char;
extern "C" GDExtensionInterfaceStringOperatorPlusEqCstr string_operator_plus_eq_cstr;
extern "C" GDExtensionInterfaceStringOperatorPlusEqWcstr string_operator_plus_eq_wcstr;
extern "C" GDExtensionInterfaceStringOperatorPlusEqC32str string_operator_plus_eq_c32str;
extern "C" GDExtensionInterfaceXmlParserOpenBuffer xml_parser_open_buffer;
extern "C" GDExtensionInterfaceFileAccessStoreBuffer file_access_store_buffer;
extern "C" GDExtensionInterfaceFileAccessGetBuffer file_access_get_buffer;
extern "C" GDExtensionInterfaceWorkerThreadPoolAddNativeGroupTask worker_thread_pool_add_native_group_task;
extern "C" GDExtensionInterfaceWorkerThreadPoolAddNativeTask worker_thread_pool_add_native_task;
extern "C" GDExtensionInterfacePackedByteArrayOperatorIndex packed_byte_array_operator_index;
extern "C" GDExtensionInterfacePackedByteArrayOperatorIndexConst packed_byte_array_operator_index_const;
extern "C" GDExtensionInterfacePackedFloat32ArrayOperatorIndex packed_float32_array_operator_index;
extern "C" GDExtensionInterfacePackedFloat32ArrayOperatorIndexConst packed_float32_array_operator_index_const;
extern "C" GDExtensionInterfacePackedFloat64ArrayOperatorIndex packed_float64_array_operator_index;
extern "C" GDExtensionInterfacePackedFloat64ArrayOperatorIndexConst packed_float64_array_operator_index_const;
extern "C" GDExtensionInterfacePackedInt32ArrayOperatorIndex packed_int32_array_operator_index;
extern "C" GDExtensionInterfacePackedInt32ArrayOperatorIndexConst packed_int32_array_operator_index_const;
extern "C" GDExtensionInterfacePackedInt64ArrayOperatorIndex packed_int64_array_operator_index;
extern "C" GDExtensionInterfacePackedInt64ArrayOperatorIndexConst packed_int64_array_operator_index_const;
extern "C" GDExtensionInterfacePackedStringArrayOperatorIndex packed_string_array_operator_index;
extern "C" GDExtensionInterfacePackedStringArrayOperatorIndexConst packed_string_array_operator_index_const;
extern "C" GDExtensionInterfacePackedVector2ArrayOperatorIndex packed_vector2_array_operator_index;
extern "C" GDExtensionInterfacePackedVector2ArrayOperatorIndexConst packed_vector2_array_operator_index_const;
extern "C" GDExtensionInterfacePackedVector3ArrayOperatorIndex packed_vector3_array_operator_index;
extern "C" GDExtensionInterfacePackedVector3ArrayOperatorIndexConst packed_vector3_array_operator_index_const;
extern "C" GDExtensionInterfacePackedColorArrayOperatorIndex packed_color_array_operator_index;
extern "C" GDExtensionInterfacePackedColorArrayOperatorIndexConst packed_color_array_operator_index_const;
extern "C" GDExtensionInterfaceArrayOperatorIndex array_operator_index;
extern "C" GDExtensionInterfaceArrayOperatorIndexConst array_operator_index_const;
#if !defined(DISABLE_DEPRECATED) || GODOT_VERSION_MINOR < 5
extern "C" GDExtensionInterfaceArrayRef array_ref;
#endif
extern "C" GDExtensionInterfaceArraySetTyped array_set_typed;
extern "C" GDExtensionInterfaceDictionaryOperatorIndex dictionary_operator_index;
extern "C" GDExtensionInterfaceDictionaryOperatorIndexConst dictionary_operator_index_const;
extern "C" GDExtensionInterfaceObjectMethodBindCall object_method_bind_call;
extern "C" GDExtensionInterfaceObjectMethodBindPtrcall object_method_bind_ptrcall;
extern "C" GDExtensionInterfaceObjectDestroy object_destroy;
extern "C" GDExtensionInterfaceGlobalGetSingleton global_get_singleton;
extern "C" GDExtensionInterfaceObjectGetInstanceBinding object_get_instance_binding;
extern "C" GDExtensionInterfaceObjectSetInstanceBinding object_set_instance_binding;
extern "C" GDExtensionInterfaceObjectSetInstance object_set_instance;
extern "C" GDExtensionInterfaceObjectGetClassName object_get_class_name;
#if !defined(DISABLE_DEPRECATED) || GODOT_VERSION_MINOR < 7
extern "C" GDExtensionInterfaceObjectCastTo object_cast_to;
#endif
extern "C" GDExtensionInterfaceObjectGetInstanceFromId object_get_instance_from_id;
extern "C" GDExtensionInterfaceObjectGetInstanceId object_get_instance_id;
extern "C" GDExtensionInterfaceRefGetObject ref_get_object;
extern "C" GDExtensionInterfaceRefSetObject ref_set_object;
#if !defined(DISABLE_DEPRECATED) || GODOT_VERSION_MINOR < 2
extern "C" GDExtensionInterfaceScriptInstanceCreate script_instance_create;
#endif
#if !defined(DISABLE_DEPRECATED) || GODOT_VERSION_MINOR < 4
extern "C" GDExtensionInterfaceClassdbConstructObject classdb_construct_object;
#endif
extern "C" GDExtensionInterfaceClassdbGetMethodBind classdb_get_method_bind;
#if !defined(DISABLE_DEPRECATED) || GODOT_VERSION_MINOR < 7
extern "C" GDExtensionInterfaceClassdbGetClassTag classdb_get_class_tag;
#endif
#if !defined(DISABLE_DEPRECATED) || GODOT_VERSION_MINOR < 2
extern "C" GDExtensionInterfaceClassdbRegisterExtensionClass classdb_register_extension_class;
#endif
extern "C" GDExtensionInterfaceClassdbRegisterExtensionClassMethod classdb_register_extension_class_method;
extern "C" GDExtensionInterfaceClassdbRegisterExtensionClassIntegerConstant classdb_register_extension_class_integer_constant;
extern "C" GDExtensionInterfaceClassdbRegisterExtensionClassProperty classdb_register_extension_class_property;
extern "C" GDExtensionInterfaceClassdbRegisterExtensionClassPropertyGroup classdb_register_extension_class_property_group;
extern "C" GDExtensionInterfaceClassdbRegisterExtensionClassPropertySubgroup classdb_register_extension_class_property_subgroup;
extern "C" GDExtensionInterfaceClassdbRegisterExtensionClassSignal classdb_register_extension_class_signal;
extern "C" GDExtensionInterfaceClassdbUnregisterExtensionClass classdb_unregister_extension_class;
extern "C" GDExtensionInterfaceGetLibraryPath get_library_path;
extern "C" GDExtensionInterfaceEditorAddPlugin editor_add_plugin;
extern "C" GDExtensionInterfaceEditorRemovePlugin editor_remove_plugin;
#endif // GODOT_VERSION_MINOR >= 1

// Godot 4.2 or newer.
#if GODOT_VERSION_MINOR >= 2
extern "C" GDExtensionInterfaceStringResize string_resize;
extern "C" GDExtensionInterfaceStringNameNewWithLatin1Chars string_name_new_with_latin1_chars;
extern "C" GDExtensionInterfaceStringNameNewWithUtf8Chars string_name_new_with_utf8_chars;
extern "C" GDExtensionInterfaceStringNameNewWithUtf8CharsAndLen string_name_new_with_utf8_chars_and_len;
extern "C" GDExtensionInterfaceObjectFreeInstanceBinding object_free_instance_binding;
#if !defined(DISABLE_DEPRECATED) || GODOT_VERSION_MINOR < 3
extern "C" GDExtensionInterfaceScriptInstanceCreate2 script_instance_create2;
#endif
extern "C" GDExtensionInterfacePlaceholderScriptInstanceCreate placeholder_script_instance_create;
extern "C" GDExtensionInterfacePlaceholderScriptInstanceUpdate placeholder_script_instance_update;
extern "C" GDExtensionInterfaceObjectGetScriptInstance object_get_script_instance;
#if !defined(DISABLE_DEPRECATED) || GODOT_VERSION_MINOR < 3
extern "C" GDExtensionInterfaceCallableCustomCreate callable_custom_create;
#endif
extern "C" GDExtensionInterfaceCallableCustomGetUserdata callable_custom_get_userdata;
#if !defined(DISABLE_DEPRECATED) || GODOT_VERSION_MINOR < 3
extern "C" GDExtensionInterfaceClassdbRegisterExtensionClass2 classdb_register_extension_class2;
#endif
extern "C" GDExtensionInterfaceClassdbRegisterExtensionClassPropertyIndexed classdb_register_extension_class_property_indexed;
#endif // GODOT_VERSION_MINOR >= 2

// Godot 4.3 or newer.
#if GODOT_VERSION_MINOR >= 3
extern "C" GDExtensionInterfaceStringNewWithUtf8CharsAndLen2 string_new_with_utf8_chars_and_len2;
extern "C" GDExtensionInterfaceStringNewWithUtf16CharsAndLen2 string_new_with_utf16_chars_and_len2;
extern "C" GDExtensionInterfaceImagePtrw image_ptrw;
extern "C" GDExtensionInterfaceImagePtr image_ptr;
extern "C" GDExtensionInterfacePackedVector4ArrayOperatorIndex packed_vector4_array_operator_index;
extern "C" GDExtensionInterfacePackedVector4ArrayOperatorIndexConst packed_vector4_array_operator_index_const;
extern "C" GDExtensionInterfaceObjectHasScriptMethod object_has_script_method;
extern "C" GDExtensionInterfaceObjectCallScriptMethod object_call_script_method;
extern "C" GDExtensionInterfaceScriptInstanceCreate3 script_instance_create3;
extern "C" GDExtensionInterfaceCallableCustomCreate2 callable_custom_create2;
#if !defined(DISABLE_DEPRECATED) || GODOT_VERSION_MINOR < 4
extern "C" GDExtensionInterfaceClassdbRegisterExtensionClass3 classdb_register_extension_class3;
#endif
extern "C" GDExtensionInterfaceClassdbRegisterExtensionClassVirtualMethod classdb_register_extension_class_virtual_method;
extern "C" GDExtensionInterfaceEditorHelpLoadXmlFromUtf8Chars editor_help_load_xml_from_utf8_chars;
extern "C" GDExtensionInterfaceEditorHelpLoadXmlFromUtf8CharsAndLen editor_help_load_xml_from_utf8_chars_and_len;
#endif // GODOT_VERSION_MINOR >= 3

// Godot 4.4 or newer.
#if GODOT_VERSION_MINOR >= 4
extern "C" GDExtensionInterfaceVariantGetObjectInstanceId variant_get_object_instance_id;
extern "C" GDExtensionInterfaceVariantGetPtrInternalGetter variant_get_ptr_internal_getter;
extern "C" GDExtensionInterfaceDictionarySetTyped dictionary_set_typed;
#if !defined(DISABLE_DEPRECATED) || GODOT_VERSION_MINOR < 7
extern "C" GDExtensionInterfaceClassdbConstructObject2 classdb_construct_object2;
#endif
#if !defined(DISABLE_DEPRECATED) || GODOT_VERSION_MINOR < 5
extern "C" GDExtensionInterfaceClassdbRegisterExtensionClass4 classdb_register_extension_class4;
#endif
#endif // GODOT_VERSION_MINOR >= 4

// Godot 4.5 or newer.
#if GODOT_VERSION_MINOR >= 5
extern "C" GDExtensionInterfaceGetGodotVersion2 get_godot_version2;
extern "C" GDExtensionInterfaceObjectSetScriptInstance object_set_script_instance;
#if !defined(DISABLE_DEPRECATED) || GODOT_VERSION_MINOR < 7
extern "C" GDExtensionInterfaceClassdbRegisterExtensionClass5 classdb_register_extension_class5;
#endif
extern "C" GDExtensionInterfaceEditorRegisterGetClassesUsedCallback editor_register_get_classes_used_callback;
extern "C" GDExtensionInterfaceRegisterMainLoopCallbacks register_main_loop_callbacks;
#endif // GODOT_VERSION_MINOR >= 5

// Godot 4.6 or newer.
#if GODOT_VERSION_MINOR >= 6
extern "C" GDExtensionInterfaceMemAlloc2 mem_alloc2;
extern "C" GDExtensionInterfaceMemRealloc2 mem_realloc2;
extern "C" GDExtensionInterfaceMemFree2 mem_free2;
#endif // GODOT_VERSION_MINOR >= 6

// Godot 4.7 or newer.
#if GODOT_VERSION_MINOR >= 7
extern "C" GDExtensionInterfaceVariantGetTypeByName variant_get_type_by_name;
extern "C" GDExtensionInterfaceClassdbConstructObject3 classdb_construct_object3;
extern "C" GDExtensionInterfaceClassdbRegisterExtensionClass6 classdb_register_extension_class6;
extern "C" GDExtensionInterfaceDeviceExitBootstrapLibraryV1 device_exit_bootstrap_library_v1;
extern "C" GDExtensionInterfaceDeviceExitRetainLibraryViewV1 device_exit_retain_library_view_v1;
extern "C" GDExtensionInterfaceDeviceExitCopyLibrarySnapshotV1 device_exit_copy_library_snapshot_v1;
extern "C" GDExtensionInterfaceDeviceExitReleaseLibraryViewV1 device_exit_release_library_view_v1;
extern "C" GDExtensionInterfaceDeviceExitCloneLibraryViewV1 device_exit_clone_library_view_v1;
extern "C" GDExtensionInterfaceDeviceExitRegisterGuardV1 device_exit_register_guard_v1;
extern "C" GDExtensionInterfaceDeviceExitBorrowGuardedLibraryV1 device_exit_borrow_guarded_library_v1;
extern "C" GDExtensionInterfaceDeviceExitBorrowInitializeLevelV1 device_exit_borrow_initialize_level_v1;
extern "C" GDExtensionInterfaceDeviceExitBorrowDeinitLevelV1 device_exit_borrow_deinit_level_v1;
extern "C" GDExtensionInterfaceDeviceExitBorrowLevelCleanupV1 device_exit_borrow_level_cleanup_v1;
extern "C" GDExtensionInterfaceDeviceExitScheduleObjectCleanupV1 device_exit_schedule_object_cleanup_v1;
extern "C" GDExtensionInterfaceDeviceExitAcquireCodeHoldV1 device_exit_acquire_code_hold_v1;
extern "C" GDExtensionInterfaceDeviceExitCloneCodeHoldV1 device_exit_clone_code_hold_v1;
extern "C" GDExtensionInterfaceDeviceExitReleaseCodeHoldV1 device_exit_release_code_hold_v1;
extern "C" GDExtensionInterfaceDeviceExitAfterBindingInitializeV1 device_exit_after_binding_initialize_v1;
extern "C" GDExtensionInterfaceDeviceExitBeginUserTerminatorV1 device_exit_begin_user_terminator_v1;
extern "C" GDExtensionInterfaceDeviceExitAfterUserTerminatorV1 device_exit_after_user_terminator_v1;
extern "C" GDExtensionInterfaceDeviceExitAfterBindingDeinitV1 device_exit_after_binding_deinit_v1;
extern "C" GDExtensionInterfaceDeviceExitPublishExitResultV1 device_exit_publish_exit_result_v1;
extern "C" GDExtensionInterfaceDeviceExitTerminateInitializeBlockedV1 device_exit_terminate_initialize_blocked_v1;
extern "C" GDExtensionInterfaceDeviceExitTerminateBlockedV1 device_exit_terminate_blocked_v1;
extern "C" GDExtensionInterfaceDeviceSessionBindV1 device_session_bind_v1;
extern "C" GDExtensionInterfaceDeviceSessionCopyV1 device_session_copy_v1;
extern "C" GDExtensionInterfaceDeviceSessionResolveV1 device_session_resolve_v1;
extern "C" GDExtensionInterfaceDeviceSessionReleaseContextV1 device_session_release_context_v1;
extern "C" GDExtensionInterfaceDeviceSessionReleaseSessionV1 device_session_release_session_v1;
extern "C" GDExtensionInterfaceDeviceExitRegisterSceneReadyV1 device_exit_register_scene_ready_v1;
extern "C" GDExtensionInterfaceDeviceExitRegisterSceneReadyV2 device_exit_register_scene_ready_v2;
extern "C" GDExtensionInterfaceDeviceExitSceneReadyScheduleObjectCleanupV1 device_exit_scene_ready_schedule_object_cleanup_v1;
extern "C" GDExtensionInterfaceDeviceSessionBindSceneReadyV1 device_session_bind_scene_ready_v1;
extern "C" GDExtensionInterfaceDeviceExitSceneReadyAbortV1 device_exit_scene_ready_abort_v1;
extern "C" GDExtensionInterfaceDeviceSessionBindMainSceneReadyV1 device_session_bind_main_scene_ready_v1;
extern "C" GDExtensionInterfaceDeviceSessionQualificationOpenV1 device_session_qualification_open_v1;
extern "C" GDExtensionInterfaceDeviceSessionQualificationBackingV1 device_session_qualification_backing_v1;
extern "C" GDExtensionInterfaceDeviceSessionQualificationRecordV1 device_session_qualification_record_v1;
extern "C" GDExtensionInterfaceDeviceSessionQualificationObserveV1 device_session_qualification_observe_v1;
extern "C" GDExtensionInterfaceDeviceSessionQualificationObserveV2 device_session_qualification_observe_v2;
extern "C" GDExtensionInterfaceDeviceSessionQualificationFactsV1 device_session_qualification_facts_v1;
extern "C" GDExtensionInterfaceDeviceSessionQualificationCopyV1 device_session_qualification_copy_v1;
extern "C" GDExtensionInterfaceDeviceSessionQualificationReleaseV1 device_session_qualification_release_v1;
extern "C" GDExtensionInterfaceDeviceSessionQualificationCloseV1 device_session_qualification_close_v1;
extern "C" GDExtensionInterfaceDeviceSessionQualificationStorageCreateV1 device_session_qualification_storage_create_v1;
extern "C" GDExtensionInterfaceDeviceSessionQualificationStorageRetireV1 device_session_qualification_storage_retire_v1;
extern "C" GDExtensionInterfaceDeviceSessionQualificationStorageRetireObserveV1 device_session_qualification_storage_retire_observe_v1;
extern "C" GDExtensionInterfaceDeviceSessionQualificationStorageRetireReleaseV1 device_session_qualification_storage_retire_release_v1;
extern "C" GDExtensionInterfaceDeviceSessionQualificationPrepareUploadsV1 device_session_qualification_prepare_uploads_v1;
extern "C" GDExtensionInterfaceDeviceSessionQualificationPrepareUploadsV2 device_session_qualification_prepare_uploads_v2;
extern "C" GDExtensionInterfaceDeviceSessionQualificationUploadV1 device_session_qualification_upload_v1;
extern "C" GDExtensionInterfaceDeviceSessionQualificationRecordPreparedV1 device_session_qualification_record_prepared_v1;
extern "C" GDExtensionInterfaceDeviceSessionQualificationRecordPreparedV2 device_session_qualification_record_prepared_v2;
extern "C" GDExtensionInterfaceDeviceSessionQualificationRecordPreparedV3 device_session_qualification_record_prepared_v3;
extern "C" GDExtensionInterfaceDeviceSessionQualificationNativeUnitCreateV1 device_session_qualification_native_unit_create_v1;
extern "C" GDExtensionInterfaceDeviceSessionQualificationNativeUnitRetireV1 device_session_qualification_native_unit_retire_v1;
extern "C" GDExtensionInterfaceDeviceSessionQualificationNativeUnitRetireObserveV1 device_session_qualification_native_unit_retire_observe_v1;
extern "C" GDExtensionInterfaceDeviceSessionQualificationNativeUnitRetireReleaseV1 device_session_qualification_native_unit_retire_release_v1;
extern "C" GDExtensionInterfaceDeviceSessionQualificationCancelV1 device_session_qualification_cancel_v1;
extern "C" GDExtensionInterfaceDeviceSessionQualificationRtConfigureV1 device_session_qualification_rt_configure_v1;
extern "C" GDExtensionInterfaceDeviceSessionQualificationRtReserveV1 device_session_qualification_rt_reserve_v1;
extern "C" GDExtensionInterfaceDeviceSessionQualificationRtActivateV1 device_session_qualification_rt_activate_v1;
extern "C" GDExtensionInterfaceDeviceSessionQualificationRtObserveV1 device_session_qualification_rt_observe_v1;
extern "C" GDExtensionInterfaceDeviceSessionQualificationRtReleaseV1 device_session_qualification_rt_release_v1;
extern "C" GDExtensionInterfaceDeviceSessionQualificationTextureCreateV1 device_session_qualification_texture_create_v1;
extern "C" GDExtensionInterfaceDeviceSessionQualificationSamplerCreateV1 device_session_qualification_sampler_create_v1;
extern "C" GDExtensionInterfaceDeviceSessionQualificationSetCreateV1 device_session_qualification_set_create_v1;
extern "C" GDExtensionInterfaceDeviceSessionQualificationTextureUploadV1 device_session_qualification_texture_upload_v1;
extern "C" GDExtensionInterfaceDeviceSessionQualificationResourceRetireV1 device_session_qualification_resource_retire_v1;
extern "C" GDExtensionInterfaceDeviceSessionQualificationResourceRetireObserveV1 device_session_qualification_resource_retire_observe_v1;
extern "C" GDExtensionInterfaceDeviceSessionQualificationResourceRetireReleaseV1 device_session_qualification_resource_retire_release_v1;
extern "C" GDExtensionInterfaceDeviceSessionBindHostProducerV1 device_session_bind_host_producer_v1;
extern "C" GDExtensionInterfaceDeviceSessionCloneProducerRegistrationV1 device_session_clone_producer_registration_v1;
extern "C" GDExtensionInterfaceDeviceSessionCopyProducerRegistrationV1 device_session_copy_producer_registration_v1;
extern "C" GDExtensionInterfaceDeviceSessionReleaseProducerRegistrationV1 device_session_release_producer_registration_v1;
extern "C" GDExtensionInterfaceDeviceSessionSystemScalarRecordV1 device_session_system_scalar_record_v1;
extern "C" GDExtensionInterfaceDeviceSessionSystemScalarObserveV1 device_session_system_scalar_observe_v1;
extern "C" GDExtensionInterfaceDeviceSessionSystemScalarFactsV1 device_session_system_scalar_facts_v1;
extern "C" GDExtensionInterfaceDeviceSessionSystemScalarCopyV1 device_session_system_scalar_copy_v1;
extern "C" GDExtensionInterfaceDeviceSessionSystemScalarCancelV1 device_session_system_scalar_cancel_v1;
extern "C" GDExtensionInterfaceDeviceSessionSystemScalarReleaseV1 device_session_system_scalar_release_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionQuoteV1 device_session_production_quote_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionAcceptV1 device_session_production_accept_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionInstallRecordReceiverV1 device_session_production_install_record_receiver_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionReserveV1 device_session_production_reserve_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionRasterReserveV1 device_session_production_raster_reserve_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionRecordScheduleV1 device_session_production_record_schedule_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionOutputCancelScheduleV1 device_session_production_output_cancel_schedule_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionRecordConsumeV1 device_session_production_record_consume_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionAppendV1 device_session_production_append_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionCloseSegmentV1 device_session_production_close_segment_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionAckSegmentV1 device_session_production_ack_segment_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionOpenV1 device_session_production_open_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionSetCreateV1 device_session_production_set_create_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionStorageCreateV1 device_session_production_storage_create_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionStorageRetireV1 device_session_production_storage_retire_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionStorageObserveV1 device_session_production_storage_observe_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionStorageReleaseV1 device_session_production_storage_release_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionImportUnitV1 device_session_production_import_unit_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionImportStorageV1 device_session_production_import_storage_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionImportSetV1 device_session_production_import_set_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionPrepareDispatchV1 device_session_production_prepare_dispatch_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionPrepareCopyV1 device_session_production_prepare_copy_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionPrepareUploadV1 device_session_production_prepare_upload_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionPrepareReadbackV1 device_session_production_prepare_readback_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionObserveV1 device_session_production_observe_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionFactsV1 device_session_production_facts_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionCopyV1 device_session_production_copy_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionCancelV1 device_session_production_cancel_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionReleaseV1 device_session_production_release_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionCloseV1 device_session_production_close_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionInstallationObserveV1 device_session_production_installation_observe_v1;
extern "C" GDExtensionInterfaceDeviceSessionQualificationRasterUnitCreateV1 device_session_qualification_raster_unit_create_v1;
extern "C" GDExtensionInterfaceDeviceSessionQualificationRasterUnitRetireV1 device_session_qualification_raster_unit_retire_v1;
extern "C" GDExtensionInterfaceDeviceSessionQualificationRasterUnitRetireObserveV1 device_session_qualification_raster_unit_retire_observe_v1;
extern "C" GDExtensionInterfaceDeviceSessionQualificationRasterUnitRetireReleaseV1 device_session_qualification_raster_unit_retire_release_v1;
extern "C" GDExtensionInterfaceDeviceSessionQualificationRecordPreparedV4 device_session_qualification_record_prepared_v4;
extern "C" GDExtensionInterfaceDeviceSessionQualificationObserveV3 device_session_qualification_observe_v3;
extern "C" GDExtensionInterfaceDeviceSessionProductionReserveV2 device_session_production_reserve_v2;
extern "C" GDExtensionInterfaceDeviceSessionProductionReserveReadbackSourcesV1 device_session_production_reserve_readback_sources_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionPrepareDispatchV2 device_session_production_prepare_dispatch_v2;
extern "C" GDExtensionInterfaceDeviceSessionProductionPrepareCopyV2 device_session_production_prepare_copy_v2;
extern "C" GDExtensionInterfaceDeviceSessionProductionPrepareUploadV2 device_session_production_prepare_upload_v2;
extern "C" GDExtensionInterfaceDeviceSessionProductionPrepareReadbackV2 device_session_production_prepare_readback_v2;
extern "C" GDExtensionInterfaceDeviceSessionProductionReadRevisionV1 device_session_production_read_revision_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionAppendV2 device_session_production_append_v2;
extern "C" GDExtensionInterfaceDeviceSessionProductionCloseSegmentV2 device_session_production_close_segment_v2;
extern "C" GDExtensionInterfaceDeviceSessionProductionRecordConsumeV2 device_session_production_record_consume_v2;
extern "C" GDExtensionInterfaceDeviceSessionProductionObserveV2 device_session_production_observe_v2;
extern "C" GDExtensionInterfaceDeviceSessionProductionFactsV2 device_session_production_facts_v2;
extern "C" GDExtensionInterfaceDeviceSessionProductionReleaseV2 device_session_production_release_v2;
extern "C" GDExtensionInterfaceDeviceSessionOutputCreateV1 device_session_output_create_v1;
extern "C" GDExtensionInterfaceDeviceSessionOutputCreateV2 device_session_output_create_v2;
extern "C" GDExtensionInterfaceDeviceSessionOutputSealV1 device_session_output_seal_v1;
extern "C" GDExtensionInterfaceDeviceSessionOutputCancelV1 device_session_output_cancel_v1;
extern "C" GDExtensionInterfaceDeviceSessionOutputWithdrawV1 device_session_output_withdraw_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionRasterBeginV1 device_session_production_raster_begin_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionRasterEndV1 device_session_production_raster_end_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionRecordHistoryV1 device_session_production_record_history_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionRasterPrepareV1 device_session_production_raster_prepare_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionRasterPrepareReportV1 device_session_production_raster_prepare_report_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionPrepareOutputV1 device_session_production_prepare_output_v1;
extern "C" GDExtensionInterfaceDeviceSessionOutputAcquireV1 device_session_output_acquire_v1;
extern "C" GDExtensionInterfaceDeviceSessionOutputPollV1 device_session_output_poll_v1;
extern "C" GDExtensionInterfaceDeviceSessionOutputCreateV3 device_session_output_create_v3;
extern "C" GDExtensionInterfaceDeviceSessionOutputMetadataQueryV1 device_session_output_metadata_query_v1;
extern "C" GDExtensionInterfaceDeviceSessionSceneOperationQueryV1 device_session_scene_operation_query_v1;
extern "C" GDExtensionInterfaceDeviceSessionSceneOperationBindV1 device_session_scene_operation_bind_v1;
extern "C" GDExtensionInterfaceDeviceSessionScenePhaseCapabilityV1 device_session_scene_phase_capability_v1;
extern "C" GDExtensionInterfaceDeviceSessionCameraReadyRelayV1 device_session_camera_ready_relay_v1;
extern "C" GDExtensionInterfaceDeviceSessionCameraReadyBindRelayV1 device_session_camera_ready_bind_relay_v1;
extern "C" GDExtensionInterfaceDeviceSessionCameraReadyBindViewportV1 device_session_camera_ready_bind_viewport_v1;
extern "C" GDExtensionInterfaceDeviceSessionCameraReadyCloseV1 device_session_camera_ready_close_v1;
extern "C" GDExtensionInterfaceDeviceSessionScenePhaseRelayV1 device_session_scene_phase_relay_v1;
extern "C" GDExtensionInterfaceDeviceSessionScenePhaseRegisterV1 device_session_scene_phase_register_v1;
extern "C" GDExtensionInterfaceDeviceSessionScenePhaseUnregisterV1 device_session_scene_phase_unregister_v1;
extern "C" GDExtensionInterfaceDeviceSessionQualificationSceneDepthQuoteV1 device_session_qualification_scene_depth_quote_v1;
extern "C" GDExtensionInterfaceDeviceSessionQualificationSceneDepthCreateV1 device_session_qualification_scene_depth_create_v1;
extern "C" GDExtensionInterfaceDeviceSessionQualificationSceneDepthRetireV1 device_session_qualification_scene_depth_retire_v1;
extern "C" GDExtensionInterfaceDeviceSessionQualificationSceneDepthObserveV1 device_session_qualification_scene_depth_observe_v1;
extern "C" GDExtensionInterfaceDeviceSessionQualificationSceneDepthReleaseV1 device_session_qualification_scene_depth_release_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionObserveV3 device_session_production_observe_v3;
extern "C" GDExtensionInterfaceDeviceSessionProductionOutputReturnConsumeV1 device_session_production_output_return_consume_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionRequestRequirementsV1 device_session_production_request_requirements_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionRequestRequirementsV2 device_session_production_request_requirements_v2;
extern "C" GDExtensionInterfaceDeviceSessionProductionCaptureCameraV1 device_session_production_capture_camera_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionRetainCameraReadyV1 device_session_production_retain_camera_ready_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionReadCameraReadyV1 device_session_production_read_camera_ready_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionReadCameraV1 device_session_production_read_camera_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionPrepareHostTransformV1 device_session_production_prepare_host_transform_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionPrepareOutputSceneV1 device_session_production_prepare_output_scene_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionReadHostTransformV1 device_session_production_read_host_transform_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionPrepareOutputPlanV1 device_session_production_prepare_output_plan_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionFinalizeOutputV1 device_session_production_finalize_output_v1;
extern "C" GDExtensionInterfaceDeviceExitRegisterPrepareV1 device_exit_register_prepare_v1;
extern "C" GDExtensionInterfaceDeviceSessionOutputConsumedBorrowV2 device_session_output_consumed_borrow_v2;
extern "C" GDExtensionInterfaceDeviceSessionOutputConsumedReadV2 device_session_output_consumed_read_v2;
extern "C" GDExtensionInterfaceDeviceSessionOutputConsumedReturnV2 device_session_output_consumed_return_v2;
extern "C" GDExtensionInterfaceDeviceSessionOutputConsumedPrepareV2 device_session_output_consumed_prepare_v2;
extern "C" GDExtensionInterfaceDeviceSessionProductionPrepareOutputV2 device_session_production_prepare_output_v2;
extern "C" GDExtensionInterfaceDeviceSessionProductionPrepareOutputPlanV2 device_session_production_prepare_output_plan_v2;
extern "C" GDExtensionInterfaceDeviceSessionOutputMetadataSelectionQueryV2 device_session_output_metadata_selection_query_v2;
extern "C" GDExtensionInterfaceDeviceSessionOutputMetadataSelectionPrepareV2 device_session_output_metadata_selection_prepare_v2;
extern "C" GDExtensionInterfaceDeviceSessionOutputMetadataSelectionReadV2 device_session_output_metadata_selection_read_v2;
extern "C" GDExtensionInterfaceDeviceSessionOutputMetadataSelectionReturnV2 device_session_output_metadata_selection_return_v2;
extern "C" GDExtensionInterfaceDeviceSessionOutputCreateV4 device_session_output_create_v4;
extern "C" GDExtensionInterfaceDeviceSessionOutputSceneQueryCommitV2 device_session_output_scene_query_commit_v2;
extern "C" GDExtensionInterfaceDeviceSessionOutputDeviceFactsV2 device_session_output_device_facts_v2;
extern "C" GDExtensionInterfaceDeviceSessionOutputSelectionFactsV2 device_session_output_selection_facts_v2;
extern "C" GDExtensionInterfaceDeviceSessionProductionPrepareNativeRecordV2 device_session_production_prepare_native_record_v2;
extern "C" GDExtensionInterfaceDeviceSessionProductionPreparedResultReadV2 device_session_production_prepared_result_read_v2;
extern "C" GDExtensionInterfaceDeviceSessionProductionStageOutputPlanV2 device_session_production_stage_output_plan_v2;
extern "C" GDExtensionInterfaceDeviceSessionProductionRecordConsumeV3 device_session_production_record_consume_v3;
extern "C" GDExtensionInterfaceDeviceSessionProductionStageCameraPayloadV2 device_session_production_stage_camera_payload_v2;
extern "C" GDExtensionInterfaceDeviceSessionProductionReadStagedOutputInputV2 device_session_production_read_staged_output_input_v2;
extern "C" GDExtensionInterfaceDeviceSessionProductionPrepareNativeBatchV2 device_session_production_prepare_native_batch_v2;
extern "C" GDExtensionInterfaceDeviceSessionProductionNativeBatchReadV2 device_session_production_native_batch_read_v2;
extern "C" GDExtensionInterfaceDeviceSessionProductionStageOutputPlanWithPredecessorV2 device_session_production_stage_output_plan_with_predecessor_v2;
extern "C" GDExtensionInterfaceDeviceSessionQualificationSceneDepthServiceReserveV2 device_session_qualification_scene_depth_service_reserve_v2;
extern "C" GDExtensionInterfaceDeviceSessionQualificationSceneDepthServiceActivateV2 device_session_qualification_scene_depth_service_activate_v2;
extern "C" GDExtensionInterfaceDeviceSessionQualificationSceneDepthServiceObserveV2 device_session_qualification_scene_depth_service_observe_v2;
extern "C" GDExtensionInterfaceDeviceSessionQualificationRecordMethodV1 device_session_qualification_record_method_v1;
extern "C" GDExtensionInterfaceDeviceSessionQualificationObserveMethodV1 device_session_qualification_observe_method_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionStageOutputPlanWithHistoryGuardV2 device_session_production_stage_output_plan_with_history_guard_v2;
extern "C" GDExtensionInterfaceDeviceSessionProductionStageOutputHistoryFallbackV2 device_session_production_stage_output_history_fallback_v2;
extern "C" GDExtensionInterfaceDeviceSessionProductionPrepareHistoryFallbackDispatchV2 device_session_production_prepare_history_fallback_dispatch_v2;
extern "C" GDExtensionInterfaceDeviceSessionProductionPrepareHistoryFallbackUploadV2 device_session_production_prepare_history_fallback_upload_v2;
extern "C" GDExtensionInterfaceDeviceSessionProductionStageOutputHistoryStaticV2 device_session_production_stage_output_history_static_v2;
#endif // GODOT_VERSION_MINOR >= 7

// Godot 4.8 or newer.
#if GODOT_VERSION_MINOR >= 8
extern "C" GDExtensionInterfaceDeviceSessionProductionControlReceiptV1 device_session_production_control_receipt_v1;
extern "C" GDExtensionInterfaceDeviceSessionCameraReadyBindRequestV1 device_session_camera_ready_bind_request_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionPrepareHostViewV1 device_session_production_prepare_host_view_v1;
extern "C" GDExtensionInterfaceDeviceSessionProductionReadHostViewV1 device_session_production_read_host_view_v1;
#endif // GODOT_VERSION_MINOR >= 8

} // namespace gdextension_interface

namespace internal {

bool load_gdextension_interface(GDExtensionInterfaceGetProcAddress p_get_proc_address);

} // namespace internal

} // namespace godot