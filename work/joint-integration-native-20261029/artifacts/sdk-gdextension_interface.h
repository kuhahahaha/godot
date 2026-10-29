/**************************************************************************/
/*  gdextension_interface.h                                               */
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

#ifndef __cplusplus
#include <stddef.h>
#include <stdint.h>

typedef uint32_t char32_t;
typedef uint16_t char16_t;
#else
#include <cstddef>
#include <cstdint>

extern "C" {
#endif

typedef enum {
	GDEXTENSION_VARIANT_TYPE_NIL = 0,
	GDEXTENSION_VARIANT_TYPE_BOOL = 1,
	GDEXTENSION_VARIANT_TYPE_INT = 2,
	GDEXTENSION_VARIANT_TYPE_FLOAT = 3,
	GDEXTENSION_VARIANT_TYPE_STRING = 4,
	GDEXTENSION_VARIANT_TYPE_VECTOR2 = 5,
	GDEXTENSION_VARIANT_TYPE_VECTOR2I = 6,
	GDEXTENSION_VARIANT_TYPE_RECT2 = 7,
	GDEXTENSION_VARIANT_TYPE_RECT2I = 8,
	GDEXTENSION_VARIANT_TYPE_VECTOR3 = 9,
	GDEXTENSION_VARIANT_TYPE_VECTOR3I = 10,
	GDEXTENSION_VARIANT_TYPE_TRANSFORM2D = 11,
	GDEXTENSION_VARIANT_TYPE_VECTOR4 = 12,
	GDEXTENSION_VARIANT_TYPE_VECTOR4I = 13,
	GDEXTENSION_VARIANT_TYPE_PLANE = 14,
	GDEXTENSION_VARIANT_TYPE_QUATERNION = 15,
	GDEXTENSION_VARIANT_TYPE_AABB = 16,
	GDEXTENSION_VARIANT_TYPE_BASIS = 17,
	GDEXTENSION_VARIANT_TYPE_TRANSFORM3D = 18,
	GDEXTENSION_VARIANT_TYPE_PROJECTION = 19,
	GDEXTENSION_VARIANT_TYPE_COLOR = 20,
	GDEXTENSION_VARIANT_TYPE_STRING_NAME = 21,
	GDEXTENSION_VARIANT_TYPE_NODE_PATH = 22,
	GDEXTENSION_VARIANT_TYPE_RID = 23,
	GDEXTENSION_VARIANT_TYPE_OBJECT = 24,
	GDEXTENSION_VARIANT_TYPE_CALLABLE = 25,
	GDEXTENSION_VARIANT_TYPE_SIGNAL = 26,
	GDEXTENSION_VARIANT_TYPE_DICTIONARY = 27,
	GDEXTENSION_VARIANT_TYPE_ARRAY = 28,
	GDEXTENSION_VARIANT_TYPE_PACKED_BYTE_ARRAY = 29,
	GDEXTENSION_VARIANT_TYPE_PACKED_INT32_ARRAY = 30,
	GDEXTENSION_VARIANT_TYPE_PACKED_INT64_ARRAY = 31,
	GDEXTENSION_VARIANT_TYPE_PACKED_FLOAT32_ARRAY = 32,
	GDEXTENSION_VARIANT_TYPE_PACKED_FLOAT64_ARRAY = 33,
	GDEXTENSION_VARIANT_TYPE_PACKED_STRING_ARRAY = 34,
	GDEXTENSION_VARIANT_TYPE_PACKED_VECTOR2_ARRAY = 35,
	GDEXTENSION_VARIANT_TYPE_PACKED_VECTOR3_ARRAY = 36,
	GDEXTENSION_VARIANT_TYPE_PACKED_COLOR_ARRAY = 37,
	GDEXTENSION_VARIANT_TYPE_PACKED_VECTOR4_ARRAY = 38,
	GDEXTENSION_VARIANT_TYPE_VARIANT_MAX = 39,
} GDExtensionVariantType;

typedef enum {
	GDEXTENSION_VARIANT_OP_EQUAL = 0,
	GDEXTENSION_VARIANT_OP_NOT_EQUAL = 1,
	GDEXTENSION_VARIANT_OP_LESS = 2,
	GDEXTENSION_VARIANT_OP_LESS_EQUAL = 3,
	GDEXTENSION_VARIANT_OP_GREATER = 4,
	GDEXTENSION_VARIANT_OP_GREATER_EQUAL = 5,
	GDEXTENSION_VARIANT_OP_ADD = 6,
	GDEXTENSION_VARIANT_OP_SUBTRACT = 7,
	GDEXTENSION_VARIANT_OP_MULTIPLY = 8,
	GDEXTENSION_VARIANT_OP_DIVIDE = 9,
	GDEXTENSION_VARIANT_OP_NEGATE = 10,
	GDEXTENSION_VARIANT_OP_POSITIVE = 11,
	GDEXTENSION_VARIANT_OP_MODULE = 12,
	GDEXTENSION_VARIANT_OP_POWER = 13,
	GDEXTENSION_VARIANT_OP_SHIFT_LEFT = 14,
	GDEXTENSION_VARIANT_OP_SHIFT_RIGHT = 15,
	GDEXTENSION_VARIANT_OP_BIT_AND = 16,
	GDEXTENSION_VARIANT_OP_BIT_OR = 17,
	GDEXTENSION_VARIANT_OP_BIT_XOR = 18,
	GDEXTENSION_VARIANT_OP_BIT_NEGATE = 19,
	GDEXTENSION_VARIANT_OP_AND = 20,
	GDEXTENSION_VARIANT_OP_OR = 21,
	GDEXTENSION_VARIANT_OP_XOR = 22,
	GDEXTENSION_VARIANT_OP_NOT = 23,
	GDEXTENSION_VARIANT_OP_IN = 24,
	GDEXTENSION_VARIANT_OP_MAX = 25,
} GDExtensionVariantOperator;

/* In this API there are multiple functions which expect the caller to pass a pointer
 * on return value as parameter.
 * In order to make it clear if the caller should initialize the return value or not
 * we have two flavor of types:
 * - `GDExtensionXXXPtr` for pointer on an initialized value
 * - `GDExtensionUninitializedXXXPtr` for pointer on uninitialized value
 *
 * Notes:
 * - Not respecting those requirements can seems harmless, but will lead to unexpected
 * segfault or memory leak (for instance with a specific compiler/OS, or when two
 * native extensions start doing ptrcall on each other).
 * - Initialization must be done with the function pointer returned by `variant_get_ptr_constructor`,
 * zero-initializing the variable should not be considered a valid initialization method here !
 * - Some types have no destructor (see `extension_api.json`'s `has_destructor` field), for
 * them it is always safe to skip the constructor for the return value if you are in a hurry ;-)
 */
typedef void *GDExtensionVariantPtr;
typedef const void *GDExtensionConstVariantPtr;
typedef void *GDExtensionUninitializedVariantPtr;
typedef void *GDExtensionStringNamePtr;
typedef const void *GDExtensionConstStringNamePtr;
typedef void *GDExtensionUninitializedStringNamePtr;
typedef void *GDExtensionStringPtr;
typedef const void *GDExtensionConstStringPtr;
typedef void *GDExtensionUninitializedStringPtr;
typedef void *GDExtensionObjectPtr;
typedef const void *GDExtensionConstObjectPtr;
typedef void *GDExtensionUninitializedObjectPtr;
typedef void *GDExtensionTypePtr;
typedef const void *GDExtensionConstTypePtr;
typedef void *GDExtensionUninitializedTypePtr;
typedef const void *GDExtensionMethodBindPtr;
typedef int64_t GDExtensionInt;
typedef uint8_t GDExtensionBool;
typedef uint64_t GDObjectInstanceID;
typedef void *GDExtensionRefPtr;
typedef const void *GDExtensionConstRefPtr;
typedef enum {
	GDEXTENSION_CALL_OK = 0,
	GDEXTENSION_CALL_ERROR_INVALID_METHOD = 1,
	/* Expected a different variant type. */
	GDEXTENSION_CALL_ERROR_INVALID_ARGUMENT = 2,
	/* Expected lower number of arguments. */
	GDEXTENSION_CALL_ERROR_TOO_MANY_ARGUMENTS = 3,
	/* Expected higher number of arguments. */
	GDEXTENSION_CALL_ERROR_TOO_FEW_ARGUMENTS = 4,
	GDEXTENSION_CALL_ERROR_INSTANCE_IS_NULL = 5,
	/* Used for const call. */
	GDEXTENSION_CALL_ERROR_METHOD_NOT_CONST = 6,
} GDExtensionCallErrorType;

typedef struct {
	GDExtensionCallErrorType error;
	int32_t argument;
	int32_t expected;
} GDExtensionCallError;

typedef void (*GDExtensionVariantFromTypeConstructorFunc)(GDExtensionUninitializedVariantPtr, GDExtensionTypePtr);
typedef void (*GDExtensionTypeFromVariantConstructorFunc)(GDExtensionUninitializedTypePtr, GDExtensionVariantPtr);
typedef void *(*GDExtensionVariantGetInternalPtrFunc)(GDExtensionVariantPtr);
typedef void (*GDExtensionPtrOperatorEvaluator)(GDExtensionConstTypePtr p_left, GDExtensionConstTypePtr p_right, GDExtensionTypePtr r_result);
typedef void (*GDExtensionPtrBuiltInMethod)(GDExtensionTypePtr p_base, const GDExtensionConstTypePtr *p_args, GDExtensionTypePtr r_return, int32_t p_argument_count);
typedef void (*GDExtensionPtrConstructor)(GDExtensionUninitializedTypePtr p_base, const GDExtensionConstTypePtr *p_args);
typedef void (*GDExtensionPtrDestructor)(GDExtensionTypePtr p_base);
typedef void (*GDExtensionPtrSetter)(GDExtensionTypePtr p_base, GDExtensionConstTypePtr p_value);
typedef void (*GDExtensionPtrGetter)(GDExtensionConstTypePtr p_base, GDExtensionTypePtr r_value);
typedef void (*GDExtensionPtrIndexedSetter)(GDExtensionTypePtr p_base, GDExtensionInt p_index, GDExtensionConstTypePtr p_value);
typedef void (*GDExtensionPtrIndexedGetter)(GDExtensionConstTypePtr p_base, GDExtensionInt p_index, GDExtensionTypePtr r_value);
typedef void (*GDExtensionPtrKeyedSetter)(GDExtensionTypePtr p_base, GDExtensionConstTypePtr p_key, GDExtensionConstTypePtr p_value);
typedef void (*GDExtensionPtrKeyedGetter)(GDExtensionConstTypePtr p_base, GDExtensionConstTypePtr p_key, GDExtensionTypePtr r_value);
typedef uint32_t (*GDExtensionPtrKeyedChecker)(GDExtensionConstVariantPtr p_base, GDExtensionConstVariantPtr p_key);
typedef void (*GDExtensionPtrUtilityFunction)(GDExtensionTypePtr r_return, const GDExtensionConstTypePtr *p_args, int32_t p_argument_count);
typedef GDExtensionObjectPtr (*GDExtensionClassConstructor)();
typedef void *(*GDExtensionInstanceBindingCreateCallback)(void *p_token, void *p_instance);
typedef void (*GDExtensionInstanceBindingFreeCallback)(void *p_token, void *p_instance, void *p_binding);
typedef GDExtensionBool (*GDExtensionInstanceBindingReferenceCallback)(void *p_token, void *p_binding, GDExtensionBool p_reference);
typedef struct {
	GDExtensionInstanceBindingCreateCallback create_callback;
	GDExtensionInstanceBindingFreeCallback free_callback;
	GDExtensionInstanceBindingReferenceCallback reference_callback;
} GDExtensionInstanceBindingCallbacks;

typedef void *GDExtensionClassInstancePtr;
typedef GDExtensionBool (*GDExtensionClassSet)(GDExtensionClassInstancePtr p_instance, GDExtensionConstStringNamePtr p_name, GDExtensionConstVariantPtr p_value);
typedef GDExtensionBool (*GDExtensionClassGet)(GDExtensionClassInstancePtr p_instance, GDExtensionConstStringNamePtr p_name, GDExtensionVariantPtr r_ret);
typedef uint64_t (*GDExtensionClassGetRID)(GDExtensionClassInstancePtr p_instance);
typedef struct {
	GDExtensionVariantType type;
	GDExtensionStringNamePtr name;
	GDExtensionStringNamePtr class_name;
	/* Bitfield of `PropertyHint` (defined in `extension_api.json`). */
	uint32_t hint;
	GDExtensionStringPtr hint_string;
	/* Bitfield of `PropertyUsageFlags` (defined in `extension_api.json`). */
	uint32_t usage;
} GDExtensionPropertyInfo;

typedef struct {
	GDExtensionStringNamePtr name;
	GDExtensionPropertyInfo return_value;
	/* Bitfield of `GDExtensionClassMethodFlags`. */
	uint32_t flags;
	int32_t id;
	/* Arguments: `default_arguments` is an array of size `argument_count`. */
	uint32_t argument_count;
	GDExtensionPropertyInfo *arguments;
	/* Default arguments: `default_arguments` is an array of size `default_argument_count`. */
	uint32_t default_argument_count;
	GDExtensionVariantPtr *default_arguments;
} GDExtensionMethodInfo;

typedef const GDExtensionPropertyInfo *(*GDExtensionClassGetPropertyList)(GDExtensionClassInstancePtr p_instance, uint32_t *r_count);
typedef void (*GDExtensionClassFreePropertyList)(GDExtensionClassInstancePtr p_instance, const GDExtensionPropertyInfo *p_list); /* Deprecated in Godot 4.3. Use `GDExtensionClassFreePropertyList2` instead. */
typedef void (*GDExtensionClassFreePropertyList2)(GDExtensionClassInstancePtr p_instance, const GDExtensionPropertyInfo *p_list, uint32_t p_count);
typedef GDExtensionBool (*GDExtensionClassPropertyCanRevert)(GDExtensionClassInstancePtr p_instance, GDExtensionConstStringNamePtr p_name);
typedef GDExtensionBool (*GDExtensionClassPropertyGetRevert)(GDExtensionClassInstancePtr p_instance, GDExtensionConstStringNamePtr p_name, GDExtensionVariantPtr r_ret);
typedef GDExtensionBool (*GDExtensionClassValidateProperty)(GDExtensionClassInstancePtr p_instance, GDExtensionPropertyInfo *p_property);
typedef void (*GDExtensionClassNotification)(GDExtensionClassInstancePtr p_instance, int32_t p_what); /* Deprecated in Godot 4.2. Use `GDExtensionClassNotification2` instead. */
typedef void (*GDExtensionClassNotification2)(GDExtensionClassInstancePtr p_instance, int32_t p_what, GDExtensionBool p_reversed);
typedef void (*GDExtensionClassToString)(GDExtensionClassInstancePtr p_instance, GDExtensionBool *r_is_valid, GDExtensionStringPtr p_out);
typedef void (*GDExtensionClassReference)(GDExtensionClassInstancePtr p_instance);
typedef void (*GDExtensionClassUnreference)(GDExtensionClassInstancePtr p_instance);
typedef void (*GDExtensionClassCallVirtual)(GDExtensionClassInstancePtr p_instance, const GDExtensionConstTypePtr *p_args, GDExtensionTypePtr r_ret);
/* Called to construct an instance of the class.
 * For classes descending from RefCounted, the reference count should be zero.
 */
typedef GDExtensionObjectPtr (*GDExtensionClassCreateInstance)(void *p_class_userdata); /* Deprecated in Godot 4.4. Use `GDExtensionClassCreateInstance3` instead. */
/* Called to construct an instance of the class.
 * For classes descending from RefCounted, the reference count should be zero.
 */
typedef GDExtensionObjectPtr (*GDExtensionClassCreateInstance2)(void *p_class_userdata, GDExtensionBool p_notify_postinitialize); /* Deprecated in Godot 4.7. Use `GDExtensionClassCreateInstance3` instead. */
/* Called to construct an instance of the class.
 * For classes descending from RefCounted, the reference count should already be incremented by 1.
 */
typedef GDExtensionObjectPtr (*GDExtensionClassCreateInstance3)(void *p_class_userdata, GDExtensionBool p_notify_postinitialize);
typedef void (*GDExtensionClassFreeInstance)(void *p_class_userdata, GDExtensionClassInstancePtr p_instance);
typedef GDExtensionClassInstancePtr (*GDExtensionClassRecreateInstance)(void *p_class_userdata, GDExtensionObjectPtr p_object);
typedef GDExtensionClassCallVirtual (*GDExtensionClassGetVirtual)(void *p_class_userdata, GDExtensionConstStringNamePtr p_name); /* Deprecated in Godot 4.4. Use `GDExtensionClassGetVirtual2` instead. */
typedef GDExtensionClassCallVirtual (*GDExtensionClassGetVirtual2)(void *p_class_userdata, GDExtensionConstStringNamePtr p_name, uint32_t p_hash);
typedef void *(*GDExtensionClassGetVirtualCallData)(void *p_class_userdata, GDExtensionConstStringNamePtr p_name); /* Deprecated in Godot 4.4. Use `GDExtensionClassGetVirtualCallData2` instead. */
typedef void *(*GDExtensionClassGetVirtualCallData2)(void *p_class_userdata, GDExtensionConstStringNamePtr p_name, uint32_t p_hash);
typedef void (*GDExtensionClassCallVirtualWithData)(GDExtensionClassInstancePtr p_instance, GDExtensionConstStringNamePtr p_name, void *p_virtual_call_userdata, const GDExtensionConstTypePtr *p_args, GDExtensionTypePtr r_ret);
typedef struct {
	GDExtensionBool is_virtual;
	GDExtensionBool is_abstract;
	GDExtensionClassSet set_func;
	GDExtensionClassGet get_func;
	GDExtensionClassGetPropertyList get_property_list_func;
	GDExtensionClassFreePropertyList free_property_list_func;
	GDExtensionClassPropertyCanRevert property_can_revert_func;
	GDExtensionClassPropertyGetRevert property_get_revert_func;
	GDExtensionClassNotification notification_func;
	GDExtensionClassToString to_string_func;
	GDExtensionClassReference reference_func;
	GDExtensionClassUnreference unreference_func;
	/* Class constructor. Required unless the class is virtual or abstract. */
	GDExtensionClassCreateInstance create_instance_func;
	/* Destructor; mandatory. */
	GDExtensionClassFreeInstance free_instance_func;
	/* Queries a virtual function by name and returns a callback to invoke the requested virtual function. */
	GDExtensionClassGetVirtual get_virtual_func;
	GDExtensionClassGetRID get_rid_func;
	/* Per-class user data, later accessible in instance bindings. */
	void *class_userdata;
} GDExtensionClassCreationInfo; /* Deprecated in Godot 4.2. Use `GDExtensionClassCreationInfo6` instead. */

typedef struct {
	GDExtensionBool is_virtual;
	GDExtensionBool is_abstract;
	GDExtensionBool is_exposed;
	GDExtensionClassSet set_func;
	GDExtensionClassGet get_func;
	GDExtensionClassGetPropertyList get_property_list_func;
	GDExtensionClassFreePropertyList free_property_list_func;
	GDExtensionClassPropertyCanRevert property_can_revert_func;
	GDExtensionClassPropertyGetRevert property_get_revert_func;
	GDExtensionClassValidateProperty validate_property_func;
	GDExtensionClassNotification2 notification_func;
	GDExtensionClassToString to_string_func;
	GDExtensionClassReference reference_func;
	GDExtensionClassUnreference unreference_func;
	/* Class constructor. Required unless the class is virtual or abstract. */
	GDExtensionClassCreateInstance create_instance_func;
	/* Destructor; mandatory. */
	GDExtensionClassFreeInstance free_instance_func;
	GDExtensionClassRecreateInstance recreate_instance_func;
	/* Queries a virtual function by name and returns a callback to invoke the requested virtual function. */
	GDExtensionClassGetVirtual get_virtual_func;
	/* Paired with `call_virtual_with_data_func`, this is an alternative to `get_virtual_func` for extensions that
	 * need or benefit from extra data when calling virtual functions.
	 * Returns user data that will be passed to `call_virtual_with_data_func`.
	 * Returning `NULL` from this function signals to Godot that the virtual function is not overridden.
	 * Data returned from this function should be managed by the extension and must be valid until the extension is deinitialized.
	 * You should supply either `get_virtual_func`, or `get_virtual_call_data_func` with `call_virtual_with_data_func`.
	 */
	GDExtensionClassGetVirtualCallData get_virtual_call_data_func;
	/* Used to call virtual functions when `get_virtual_call_data_func` is not null. */
	GDExtensionClassCallVirtualWithData call_virtual_with_data_func;
	GDExtensionClassGetRID get_rid_func;
	/* Per-class user data, later accessible in instance bindings. */
	void *class_userdata;
} GDExtensionClassCreationInfo2; /* Deprecated in Godot 4.3. Use `GDExtensionClassCreationInfo6` instead. */

typedef struct {
	GDExtensionBool is_virtual;
	GDExtensionBool is_abstract;
	GDExtensionBool is_exposed;
	GDExtensionBool is_runtime;
	GDExtensionClassSet set_func;
	GDExtensionClassGet get_func;
	GDExtensionClassGetPropertyList get_property_list_func;
	GDExtensionClassFreePropertyList2 free_property_list_func;
	GDExtensionClassPropertyCanRevert property_can_revert_func;
	GDExtensionClassPropertyGetRevert property_get_revert_func;
	GDExtensionClassValidateProperty validate_property_func;
	GDExtensionClassNotification2 notification_func;
	GDExtensionClassToString to_string_func;
	GDExtensionClassReference reference_func;
	GDExtensionClassUnreference unreference_func;
	/* Class constructor. Required unless the class is virtual or abstract. */
	GDExtensionClassCreateInstance create_instance_func;
	/* Destructor; mandatory. */
	GDExtensionClassFreeInstance free_instance_func;
	GDExtensionClassRecreateInstance recreate_instance_func;
	/* Queries a virtual function by name and returns a callback to invoke the requested virtual function. */
	GDExtensionClassGetVirtual get_virtual_func;
	/* Paired with `call_virtual_with_data_func`, this is an alternative to `get_virtual_func` for extensions that
	 * need or benefit from extra data when calling virtual functions.
	 * Returns user data that will be passed to `call_virtual_with_data_func`.
	 * Returning `NULL` from this function signals to Godot that the virtual function is not overridden.
	 * Data returned from this function should be managed by the extension and must be valid until the extension is deinitialized.
	 * You should supply either `get_virtual_func`, or `get_virtual_call_data_func` with `call_virtual_with_data_func`.
	 */
	GDExtensionClassGetVirtualCallData get_virtual_call_data_func;
	/* Used to call virtual functions when `get_virtual_call_data_func` is not null. */
	GDExtensionClassCallVirtualWithData call_virtual_with_data_func;
	GDExtensionClassGetRID get_rid_func;
	/* Per-class user data, later accessible in instance bindings. */
	void *class_userdata;
} GDExtensionClassCreationInfo3; /* Deprecated in Godot 4.4. Use `GDExtensionClassCreationInfo6` instead. */

typedef struct {
	GDExtensionBool is_virtual;
	GDExtensionBool is_abstract;
	GDExtensionBool is_exposed;
	GDExtensionBool is_runtime;
	GDExtensionConstStringPtr icon_path;
	GDExtensionClassSet set_func;
	GDExtensionClassGet get_func;
	GDExtensionClassGetPropertyList get_property_list_func;
	GDExtensionClassFreePropertyList2 free_property_list_func;
	GDExtensionClassPropertyCanRevert property_can_revert_func;
	GDExtensionClassPropertyGetRevert property_get_revert_func;
	GDExtensionClassValidateProperty validate_property_func;
	GDExtensionClassNotification2 notification_func;
	GDExtensionClassToString to_string_func;
	GDExtensionClassReference reference_func;
	GDExtensionClassUnreference unreference_func;
	/* Class constructor. Required unless the class is virtual or abstract. */
	GDExtensionClassCreateInstance2 create_instance_func;
	/* Destructor; mandatory. */
	GDExtensionClassFreeInstance free_instance_func;
	GDExtensionClassRecreateInstance recreate_instance_func;
	/* Queries a virtual function by name and returns a callback to invoke the requested virtual function. */
	GDExtensionClassGetVirtual2 get_virtual_func;
	/* Paired with `call_virtual_with_data_func`, this is an alternative to `get_virtual_func` for extensions that
	 * need or benefit from extra data when calling virtual functions.
	 * Returns user data that will be passed to `call_virtual_with_data_func`.
	 * Returning `NULL` from this function signals to Godot that the virtual function is not overridden.
	 * Data returned from this function should be managed by the extension and must be valid until the extension is deinitialized.
	 * You should supply either `get_virtual_func`, or `get_virtual_call_data_func` with `call_virtual_with_data_func`.
	 */
	GDExtensionClassGetVirtualCallData2 get_virtual_call_data_func;
	/* Used to call virtual functions when `get_virtual_call_data_func` is not null. */
	GDExtensionClassCallVirtualWithData call_virtual_with_data_func;
	/* Per-class user data, later accessible in instance bindings. */
	void *class_userdata;
} GDExtensionClassCreationInfo4; /* Deprecated in Godot 4.5. Use `GDExtensionClassCreationInfo6` instead. */

typedef GDExtensionClassCreationInfo4 GDExtensionClassCreationInfo5; /* Deprecated in Godot 4.7. Use `GDExtensionClassCreationInfo6` instead. */
typedef struct {
	GDExtensionBool is_virtual;
	GDExtensionBool is_abstract;
	GDExtensionBool is_exposed;
	GDExtensionBool is_runtime;
	GDExtensionConstStringPtr icon_path;
	GDExtensionClassSet set_func;
	GDExtensionClassGet get_func;
	GDExtensionClassGetPropertyList get_property_list_func;
	GDExtensionClassFreePropertyList2 free_property_list_func;
	GDExtensionClassPropertyCanRevert property_can_revert_func;
	GDExtensionClassPropertyGetRevert property_get_revert_func;
	GDExtensionClassValidateProperty validate_property_func;
	GDExtensionClassNotification2 notification_func;
	GDExtensionClassToString to_string_func;
	GDExtensionClassReference reference_func;
	GDExtensionClassUnreference unreference_func;
	/* Class constructor. Required unless the class is virtual or abstract. */
	GDExtensionClassCreateInstance3 create_instance_func;
	/* Destructor; mandatory. */
	GDExtensionClassFreeInstance free_instance_func;
	GDExtensionClassRecreateInstance recreate_instance_func;
	/* Queries a virtual function by name and returns a callback to invoke the requested virtual function. */
	GDExtensionClassGetVirtual2 get_virtual_func;
	/* Paired with `call_virtual_with_data_func`, this is an alternative to `get_virtual_func` for extensions that
	 * need or benefit from extra data when calling virtual functions.
	 * Returns user data that will be passed to `call_virtual_with_data_func`.
	 * Returning `NULL` from this function signals to Godot that the virtual function is not overridden.
	 * Data returned from this function should be managed by the extension and must be valid until the extension is deinitialized.
	 * You should supply either `get_virtual_func`, or `get_virtual_call_data_func` with `call_virtual_with_data_func`.
	 */
	GDExtensionClassGetVirtualCallData2 get_virtual_call_data_func;
	/* Used to call virtual functions when `get_virtual_call_data_func` is not null. */
	GDExtensionClassCallVirtualWithData call_virtual_with_data_func;
	/* Per-class user data, later accessible in instance bindings. */
	void *class_userdata;
} GDExtensionClassCreationInfo6;

typedef void *GDExtensionClassLibraryPtr;
/* Passed a pointer to a PackedStringArray that should be filled with the classes that may be used by the GDExtension. */
typedef void (*GDExtensionEditorGetClassesUsedCallback)(GDExtensionTypePtr p_packed_string_array);
typedef enum {
	GDEXTENSION_METHOD_FLAG_NORMAL = 1,
	GDEXTENSION_METHOD_FLAG_EDITOR = 2,
	GDEXTENSION_METHOD_FLAG_CONST = 4,
	GDEXTENSION_METHOD_FLAG_VIRTUAL = 8,
	GDEXTENSION_METHOD_FLAG_VARARG = 16,
	GDEXTENSION_METHOD_FLAG_STATIC = 32,
	GDEXTENSION_METHOD_FLAG_VIRTUAL_REQUIRED = 128,
	GDEXTENSION_METHOD_FLAGS_DEFAULT = 1,
} GDExtensionClassMethodFlags;

typedef enum {
	GDEXTENSION_METHOD_ARGUMENT_METADATA_NONE = 0,
	GDEXTENSION_METHOD_ARGUMENT_METADATA_INT_IS_INT8 = 1,
	GDEXTENSION_METHOD_ARGUMENT_METADATA_INT_IS_INT16 = 2,
	GDEXTENSION_METHOD_ARGUMENT_METADATA_INT_IS_INT32 = 3,
	GDEXTENSION_METHOD_ARGUMENT_METADATA_INT_IS_INT64 = 4,
	GDEXTENSION_METHOD_ARGUMENT_METADATA_INT_IS_UINT8 = 5,
	GDEXTENSION_METHOD_ARGUMENT_METADATA_INT_IS_UINT16 = 6,
	GDEXTENSION_METHOD_ARGUMENT_METADATA_INT_IS_UINT32 = 7,
	GDEXTENSION_METHOD_ARGUMENT_METADATA_INT_IS_UINT64 = 8,
	GDEXTENSION_METHOD_ARGUMENT_METADATA_REAL_IS_FLOAT = 9,
	GDEXTENSION_METHOD_ARGUMENT_METADATA_REAL_IS_DOUBLE = 10,
	GDEXTENSION_METHOD_ARGUMENT_METADATA_INT_IS_CHAR16 = 11,
	GDEXTENSION_METHOD_ARGUMENT_METADATA_INT_IS_CHAR32 = 12,
	GDEXTENSION_METHOD_ARGUMENT_METADATA_OBJECT_IS_REQUIRED = 13,
} GDExtensionClassMethodArgumentMetadata;

typedef void (*GDExtensionClassMethodCall)(void *method_userdata, GDExtensionClassInstancePtr p_instance, const GDExtensionConstVariantPtr *p_args, GDExtensionInt p_argument_count, GDExtensionVariantPtr r_return, GDExtensionCallError *r_error);
typedef void (*GDExtensionClassMethodValidatedCall)(void *method_userdata, GDExtensionClassInstancePtr p_instance, const GDExtensionConstVariantPtr *p_args, GDExtensionVariantPtr r_return);
typedef void (*GDExtensionClassMethodPtrCall)(void *method_userdata, GDExtensionClassInstancePtr p_instance, const GDExtensionConstTypePtr *p_args, GDExtensionTypePtr r_ret);
typedef struct {
	GDExtensionStringNamePtr name;
	void *method_userdata;
	GDExtensionClassMethodCall call_func;
	GDExtensionClassMethodPtrCall ptrcall_func;
	/* Bitfield of `GDExtensionClassMethodFlags`. */
	uint32_t method_flags;
	/* If `has_return_value` is false, `return_value_info` and `return_value_metadata` are ignored.
	 *
	 * @todo Consider dropping `has_return_value` and making the other two properties match `GDExtensionMethodInfo` and `GDExtensionClassVirtualMethod` for consistency in future version of this struct.
	 */
	GDExtensionBool has_return_value;
	GDExtensionPropertyInfo *return_value_info;
	GDExtensionClassMethodArgumentMetadata return_value_metadata;
	/* Arguments: `arguments_info` and `arguments_metadata` are array of size `argument_count`.
	 * Name and hint information for the argument can be omitted in release builds. Class name should always be present if it applies.
	 *
	 * @todo Consider renaming `arguments_info` to `arguments` for consistency in future version of this struct.
	 */
	uint32_t argument_count;
	GDExtensionPropertyInfo *arguments_info;
	GDExtensionClassMethodArgumentMetadata *arguments_metadata;
	/* Default arguments: `default_arguments` is an array of size `default_argument_count`. */
	uint32_t default_argument_count;
	GDExtensionVariantPtr *default_arguments;
} GDExtensionClassMethodInfo;

typedef struct {
	GDExtensionStringNamePtr name;
	/* Bitfield of `GDExtensionClassMethodFlags`. */
	uint32_t method_flags;
	GDExtensionPropertyInfo return_value;
	GDExtensionClassMethodArgumentMetadata return_value_metadata;
	uint32_t argument_count;
	GDExtensionPropertyInfo *arguments;
	GDExtensionClassMethodArgumentMetadata *arguments_metadata;
} GDExtensionClassVirtualMethodInfo;

typedef void (*GDExtensionCallableCustomCall)(void *callable_userdata, const GDExtensionConstVariantPtr *p_args, GDExtensionInt p_argument_count, GDExtensionVariantPtr r_return, GDExtensionCallError *r_error);
typedef GDExtensionBool (*GDExtensionCallableCustomIsValid)(void *callable_userdata);
typedef void (*GDExtensionCallableCustomFree)(void *callable_userdata);
typedef uint32_t (*GDExtensionCallableCustomHash)(void *callable_userdata);
typedef GDExtensionBool (*GDExtensionCallableCustomEqual)(void *callable_userdata_a, void *callable_userdata_b);
typedef GDExtensionBool (*GDExtensionCallableCustomLessThan)(void *callable_userdata_a, void *callable_userdata_b);
typedef void (*GDExtensionCallableCustomToString)(void *callable_userdata, GDExtensionBool *r_is_valid, GDExtensionStringPtr r_out);
typedef GDExtensionInt (*GDExtensionCallableCustomGetArgumentCount)(void *callable_userdata, GDExtensionBool *r_is_valid);
/* Only `call_func` and `token` are strictly required, however, `object_id` should be passed if its not a static method.
 *
 * `token` should point to an address that uniquely identifies the GDExtension (for example, the
 * `GDExtensionClassLibraryPtr` passed to the entry symbol function.
 *
 * `hash_func`, `equal_func`, and `less_than_func` are optional. If not provided both `call_func` and
 * `callable_userdata` together are used as the identity of the callable for hashing and comparison purposes.
 *
 * The hash returned by `hash_func` is cached, `hash_func` will not be called more than once per callable.
 *
 * `is_valid_func` is necessary if the validity of the callable can change before destruction.
 *
 * `free_func` is necessary if `callable_userdata` needs to be cleaned up when the callable is freed.
 */
typedef struct {
	void *callable_userdata;
	void *token;
	GDObjectInstanceID object_id;
	GDExtensionCallableCustomCall call_func;
	GDExtensionCallableCustomIsValid is_valid_func;
	GDExtensionCallableCustomFree free_func;
	GDExtensionCallableCustomHash hash_func;
	GDExtensionCallableCustomEqual equal_func;
	GDExtensionCallableCustomLessThan less_than_func;
	GDExtensionCallableCustomToString to_string_func;
} GDExtensionCallableCustomInfo; /* Deprecated in Godot 4.3. Use `GDExtensionCallableCustomInfo2` instead. */

/* Only `call_func` and `token` are strictly required, however, `object_id` should be passed if its not a static method.
 *
 * `token` should point to an address that uniquely identifies the GDExtension (for example, the
 * `GDExtensionClassLibraryPtr` passed to the entry symbol function.
 *
 * `hash_func`, `equal_func`, and `less_than_func` are optional. If not provided both `call_func` and
 * `callable_userdata` together are used as the identity of the callable for hashing and comparison purposes.
 *
 * The hash returned by `hash_func` is cached, `hash_func` will not be called more than once per callable.
 *
 * `is_valid_func` is necessary if the validity of the callable can change before destruction.
 *
 * `free_func` is necessary if `callable_userdata` needs to be cleaned up when the callable is freed.
 */
typedef struct {
	void *callable_userdata;
	void *token;
	GDObjectInstanceID object_id;
	GDExtensionCallableCustomCall call_func;
	GDExtensionCallableCustomIsValid is_valid_func;
	GDExtensionCallableCustomFree free_func;
	GDExtensionCallableCustomHash hash_func;
	GDExtensionCallableCustomEqual equal_func;
	GDExtensionCallableCustomLessThan less_than_func;
	GDExtensionCallableCustomToString to_string_func;
	GDExtensionCallableCustomGetArgumentCount get_argument_count_func;
} GDExtensionCallableCustomInfo2;

/* Pointer to custom ScriptInstance native implementation. */
typedef void *GDExtensionScriptInstanceDataPtr;
typedef GDExtensionBool (*GDExtensionScriptInstanceSet)(GDExtensionScriptInstanceDataPtr p_instance, GDExtensionConstStringNamePtr p_name, GDExtensionConstVariantPtr p_value);
typedef GDExtensionBool (*GDExtensionScriptInstanceGet)(GDExtensionScriptInstanceDataPtr p_instance, GDExtensionConstStringNamePtr p_name, GDExtensionVariantPtr r_ret);
typedef const GDExtensionPropertyInfo *(*GDExtensionScriptInstanceGetPropertyList)(GDExtensionScriptInstanceDataPtr p_instance, uint32_t *r_count);
typedef void (*GDExtensionScriptInstanceFreePropertyList)(GDExtensionScriptInstanceDataPtr p_instance, const GDExtensionPropertyInfo *p_list); /* Deprecated in Godot 4.3. Use `GDExtensionScriptInstanceFreePropertyList2` instead. */
typedef void (*GDExtensionScriptInstanceFreePropertyList2)(GDExtensionScriptInstanceDataPtr p_instance, const GDExtensionPropertyInfo *p_list, uint32_t p_count);
typedef GDExtensionBool (*GDExtensionScriptInstanceGetClassCategory)(GDExtensionScriptInstanceDataPtr p_instance, GDExtensionPropertyInfo *p_class_category);
typedef GDExtensionVariantType (*GDExtensionScriptInstanceGetPropertyType)(GDExtensionScriptInstanceDataPtr p_instance, GDExtensionConstStringNamePtr p_name, GDExtensionBool *r_is_valid);
typedef GDExtensionBool (*GDExtensionScriptInstanceValidateProperty)(GDExtensionScriptInstanceDataPtr p_instance, GDExtensionPropertyInfo *p_property);
typedef GDExtensionBool (*GDExtensionScriptInstancePropertyCanRevert)(GDExtensionScriptInstanceDataPtr p_instance, GDExtensionConstStringNamePtr p_name);
typedef GDExtensionBool (*GDExtensionScriptInstancePropertyGetRevert)(GDExtensionScriptInstanceDataPtr p_instance, GDExtensionConstStringNamePtr p_name, GDExtensionVariantPtr r_ret);
typedef GDExtensionObjectPtr (*GDExtensionScriptInstanceGetOwner)(GDExtensionScriptInstanceDataPtr p_instance);
typedef void (*GDExtensionScriptInstancePropertyStateAdd)(GDExtensionConstStringNamePtr p_name, GDExtensionConstVariantPtr p_value, void *p_userdata);
typedef void (*GDExtensionScriptInstanceGetPropertyState)(GDExtensionScriptInstanceDataPtr p_instance, GDExtensionScriptInstancePropertyStateAdd p_add_func, void *p_userdata);
typedef const GDExtensionMethodInfo *(*GDExtensionScriptInstanceGetMethodList)(GDExtensionScriptInstanceDataPtr p_instance, uint32_t *r_count);
typedef void (*GDExtensionScriptInstanceFreeMethodList)(GDExtensionScriptInstanceDataPtr p_instance, const GDExtensionMethodInfo *p_list); /* Deprecated in Godot 4.3. Use `GDExtensionScriptInstanceFreeMethodList2` instead. */
typedef void (*GDExtensionScriptInstanceFreeMethodList2)(GDExtensionScriptInstanceDataPtr p_instance, const GDExtensionMethodInfo *p_list, uint32_t p_count);
typedef GDExtensionBool (*GDExtensionScriptInstanceHasMethod)(GDExtensionScriptInstanceDataPtr p_instance, GDExtensionConstStringNamePtr p_name);
typedef GDExtensionInt (*GDExtensionScriptInstanceGetMethodArgumentCount)(GDExtensionScriptInstanceDataPtr p_instance, GDExtensionConstStringNamePtr p_name, GDExtensionBool *r_is_valid);
typedef void (*GDExtensionScriptInstanceCall)(GDExtensionScriptInstanceDataPtr p_self, GDExtensionConstStringNamePtr p_method, const GDExtensionConstVariantPtr *p_args, GDExtensionInt p_argument_count, GDExtensionVariantPtr r_return, GDExtensionCallError *r_error);
typedef void (*GDExtensionScriptInstanceNotification)(GDExtensionScriptInstanceDataPtr p_instance, int32_t p_what); /* Deprecated in Godot 4.2. Use `GDExtensionScriptInstanceNotification2` instead. */
typedef void (*GDExtensionScriptInstanceNotification2)(GDExtensionScriptInstanceDataPtr p_instance, int32_t p_what, GDExtensionBool p_reversed);
typedef void (*GDExtensionScriptInstanceToString)(GDExtensionScriptInstanceDataPtr p_instance, GDExtensionBool *r_is_valid, GDExtensionStringPtr r_out);
typedef void (*GDExtensionScriptInstanceRefCountIncremented)(GDExtensionScriptInstanceDataPtr p_instance);
typedef GDExtensionBool (*GDExtensionScriptInstanceRefCountDecremented)(GDExtensionScriptInstanceDataPtr p_instance);
typedef GDExtensionObjectPtr (*GDExtensionScriptInstanceGetScript)(GDExtensionScriptInstanceDataPtr p_instance);
typedef GDExtensionBool (*GDExtensionScriptInstanceIsPlaceholder)(GDExtensionScriptInstanceDataPtr p_instance);
typedef void *GDExtensionScriptLanguagePtr;
typedef GDExtensionScriptLanguagePtr (*GDExtensionScriptInstanceGetLanguage)(GDExtensionScriptInstanceDataPtr p_instance);
typedef void (*GDExtensionScriptInstanceFree)(GDExtensionScriptInstanceDataPtr p_instance);
/* Pointer to ScriptInstance. */
typedef void *GDExtensionScriptInstancePtr;
typedef struct {
	GDExtensionScriptInstanceSet set_func;
	GDExtensionScriptInstanceGet get_func;
	GDExtensionScriptInstanceGetPropertyList get_property_list_func;
	GDExtensionScriptInstanceFreePropertyList free_property_list_func;
	GDExtensionScriptInstancePropertyCanRevert property_can_revert_func;
	GDExtensionScriptInstancePropertyGetRevert property_get_revert_func;
	GDExtensionScriptInstanceGetOwner get_owner_func;
	GDExtensionScriptInstanceGetPropertyState get_property_state_func;
	GDExtensionScriptInstanceGetMethodList get_method_list_func;
	GDExtensionScriptInstanceFreeMethodList free_method_list_func;
	GDExtensionScriptInstanceGetPropertyType get_property_type_func;
	GDExtensionScriptInstanceHasMethod has_method_func;
	GDExtensionScriptInstanceCall call_func;
	GDExtensionScriptInstanceNotification notification_func;
	GDExtensionScriptInstanceToString to_string_func;
	GDExtensionScriptInstanceRefCountIncremented refcount_incremented_func;
	GDExtensionScriptInstanceRefCountDecremented refcount_decremented_func;
	GDExtensionScriptInstanceGetScript get_script_func;
	GDExtensionScriptInstanceIsPlaceholder is_placeholder_func;
	GDExtensionScriptInstanceSet set_fallback_func;
	GDExtensionScriptInstanceGet get_fallback_func;
	GDExtensionScriptInstanceGetLanguage get_language_func;
	GDExtensionScriptInstanceFree free_func;
} GDExtensionScriptInstanceInfo; /* Deprecated in Godot 4.2. Use `GDExtensionScriptInstanceInfo3` instead. */

typedef struct {
	GDExtensionScriptInstanceSet set_func;
	GDExtensionScriptInstanceGet get_func;
	GDExtensionScriptInstanceGetPropertyList get_property_list_func;
	GDExtensionScriptInstanceFreePropertyList free_property_list_func;
	/* Optional. Set to NULL for the default behavior. */
	GDExtensionScriptInstanceGetClassCategory get_class_category_func;
	GDExtensionScriptInstancePropertyCanRevert property_can_revert_func;
	GDExtensionScriptInstancePropertyGetRevert property_get_revert_func;
	GDExtensionScriptInstanceGetOwner get_owner_func;
	GDExtensionScriptInstanceGetPropertyState get_property_state_func;
	GDExtensionScriptInstanceGetMethodList get_method_list_func;
	GDExtensionScriptInstanceFreeMethodList free_method_list_func;
	GDExtensionScriptInstanceGetPropertyType get_property_type_func;
	GDExtensionScriptInstanceValidateProperty validate_property_func;
	GDExtensionScriptInstanceHasMethod has_method_func;
	GDExtensionScriptInstanceCall call_func;
	GDExtensionScriptInstanceNotification2 notification_func;
	GDExtensionScriptInstanceToString to_string_func;
	GDExtensionScriptInstanceRefCountIncremented refcount_incremented_func;
	GDExtensionScriptInstanceRefCountDecremented refcount_decremented_func;
	GDExtensionScriptInstanceGetScript get_script_func;
	GDExtensionScriptInstanceIsPlaceholder is_placeholder_func;
	GDExtensionScriptInstanceSet set_fallback_func;
	GDExtensionScriptInstanceGet get_fallback_func;
	GDExtensionScriptInstanceGetLanguage get_language_func;
	GDExtensionScriptInstanceFree free_func;
} GDExtensionScriptInstanceInfo2; /* Deprecated in Godot 4.3. Use `GDExtensionScriptInstanceInfo3` instead. */

typedef struct {
	GDExtensionScriptInstanceSet set_func;
	GDExtensionScriptInstanceGet get_func;
	GDExtensionScriptInstanceGetPropertyList get_property_list_func;
	GDExtensionScriptInstanceFreePropertyList2 free_property_list_func;
	/* Optional. Set to NULL for the default behavior. */
	GDExtensionScriptInstanceGetClassCategory get_class_category_func;
	GDExtensionScriptInstancePropertyCanRevert property_can_revert_func;
	GDExtensionScriptInstancePropertyGetRevert property_get_revert_func;
	GDExtensionScriptInstanceGetOwner get_owner_func;
	GDExtensionScriptInstanceGetPropertyState get_property_state_func;
	GDExtensionScriptInstanceGetMethodList get_method_list_func;
	GDExtensionScriptInstanceFreeMethodList2 free_method_list_func;
	GDExtensionScriptInstanceGetPropertyType get_property_type_func;
	GDExtensionScriptInstanceValidateProperty validate_property_func;
	GDExtensionScriptInstanceHasMethod has_method_func;
	GDExtensionScriptInstanceGetMethodArgumentCount get_method_argument_count_func;
	GDExtensionScriptInstanceCall call_func;
	GDExtensionScriptInstanceNotification2 notification_func;
	GDExtensionScriptInstanceToString to_string_func;
	GDExtensionScriptInstanceRefCountIncremented refcount_incremented_func;
	GDExtensionScriptInstanceRefCountDecremented refcount_decremented_func;
	GDExtensionScriptInstanceGetScript get_script_func;
	GDExtensionScriptInstanceIsPlaceholder is_placeholder_func;
	GDExtensionScriptInstanceSet set_fallback_func;
	GDExtensionScriptInstanceGet get_fallback_func;
	GDExtensionScriptInstanceGetLanguage get_language_func;
	GDExtensionScriptInstanceFree free_func;
} GDExtensionScriptInstanceInfo3;

typedef void (*GDExtensionWorkerThreadPoolGroupTask)(void *, uint32_t);
typedef void (*GDExtensionWorkerThreadPoolTask)(void *);
typedef enum {
	GDEXTENSION_INITIALIZATION_CORE = 0,
	GDEXTENSION_INITIALIZATION_SERVERS = 1,
	GDEXTENSION_INITIALIZATION_SCENE = 2,
	GDEXTENSION_INITIALIZATION_EDITOR = 3,
	GDEXTENSION_MAX_INITIALIZATION_LEVEL = 4,
} GDExtensionInitializationLevel;

typedef void (*GDExtensionInitializeCallback)(void *p_userdata, GDExtensionInitializationLevel p_level);
typedef void (*GDExtensionDeinitializeCallback)(void *p_userdata, GDExtensionInitializationLevel p_level);
typedef struct {
	/* Minimum initialization level required.
	 * If Core or Servers, the extension needs editor or game restart to take effect
	 */
	GDExtensionInitializationLevel minimum_initialization_level;
	/* Up to the user to supply when initializing */
	void *userdata;
	/* This function will be called multiple times for each initialization level. */
	GDExtensionInitializeCallback initialize;
	GDExtensionDeinitializeCallback deinitialize;
} GDExtensionInitialization;

typedef void (*GDExtensionInterfaceFunctionPtr)();
typedef GDExtensionInterfaceFunctionPtr (*GDExtensionInterfaceGetProcAddress)(const char *p_function_name);
/* Each GDExtension should define a C function that matches the signature of GDExtensionInitializationFunction,
 * and export it so that it can be loaded via dlopen() or equivalent for the given platform.
 *
 * For example:
 *
 *   GDExtensionBool my_extension_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization);
 *
 * This function's name must be specified as the 'entry_symbol' in the .gdextension file.
 *
 * This makes it the entry point of the GDExtension and will be called on initialization.
 *
 * The GDExtension can then modify the r_initialization structure, setting the minimum initialization level,
 * and providing pointers to functions that will be called at various stages of initialization/shutdown.
 *
 * The rest of the GDExtension's interface to Godot consists of function pointers that can be loaded
 * by calling p_get_proc_address("...") with the name of the function.
 *
 * For example:
 *
 *   GDExtensionInterfaceGetGodotVersion get_godot_version = (GDExtensionInterfaceGetGodotVersion)p_get_proc_address("get_godot_version");
 *
 * (Note that snippet may cause "cast between incompatible function types" on some compilers, you can
 * silence this by adding an intermediary `void*` cast.)
 *
 * You can then call it like a normal function:
 *
 *   GDExtensionGodotVersion godot_version;
 *   get_godot_version(&godot_version);
 *   printf("Godot v%d.%d.%d\n", godot_version.major, godot_version.minor, godot_version.patch);
 *
 * All of these interface functions are described below, together with the name that's used to load it,
 * and the function pointer typedef that shows its signature.
 */
typedef GDExtensionBool (*GDExtensionInitializationFunction)(GDExtensionInterfaceGetProcAddress p_get_proc_address, GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization);
typedef struct {
	uint32_t major;
	uint32_t minor;
	uint32_t patch;
	const char *string;
} GDExtensionGodotVersion; /* Deprecated in Godot 4.5. Use `GDExtensionGodotVersion2` instead. */

typedef struct {
	uint32_t major;
	uint32_t minor;
	uint32_t patch;
	/* Full version encoded as hexadecimal with one byte (2 hex digits) per number (e.g. for "3.1.12" it would be 0x03010C) */
	uint32_t hex;
	/* (e.g. "stable", "beta", "rc1", "rc2") */
	const char *status;
	/* (e.g. "custom_build") */
	const char *build;
	/* Full Git commit hash. */
	const char *hash;
	/* Git commit date UNIX timestamp in seconds, or 0 if unavailable. */
	uint64_t timestamp;
	/* (e.g. "Godot v3.1.4.stable.official.mono") */
	const char *string;
} GDExtensionGodotVersion2;

/* Called when starting the main loop. */
typedef void (*GDExtensionMainLoopStartupCallback)();
/* Called when shutting down the main loop. */
typedef void (*GDExtensionMainLoopShutdownCallback)();
/* Called for every frame iteration of the main loop. */
typedef void (*GDExtensionMainLoopFrameCallback)();
typedef struct {
	/* Will be called after Godot is started and is fully initialized. */
	GDExtensionMainLoopStartupCallback startup_func;
	/* Will be called before Godot is shutdown when it is still fully initialized. */
	GDExtensionMainLoopShutdownCallback shutdown_func;
	/* Will be called for each process frame. This will run after all `_process()` methods on Node, and before `ScriptServer::frame()`.
	 * This is intended to be the equivalent of `ScriptLanguage::frame()` for GDExtension language bindings that don't use the script API.
	 */
	GDExtensionMainLoopFrameCallback frame_func;
} GDExtensionMainLoopCallbacks;

typedef struct {
	uint32_t abi_version;
	uint32_t struct_size;
	uint64_t reserved;
} GDExtensionDeviceExitAbi;

typedef struct {
	uint64_t registry_instance;
	uint64_t handle_serial;
} GDExtensionDeviceExitLibraryCapability;

typedef struct {
	uint64_t registry_instance;
	uint64_t handle_serial;
} GDExtensionDeviceExitLibraryViewRef;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t library_generation;
	uint32_t loader_kind;
	uint32_t wrapper_association;
	uint32_t loader_initialize_returned;
	int32_t loader_initialize_error;
	uint32_t guarded_qualified;
	uint32_t reserved;
} GDExtensionDeviceExitLibrarySnapshot;

typedef struct {
	uint64_t registry_instance;
	uint64_t handle_serial;
} GDExtensionDeviceExitGuardToken;

typedef struct {
	uint64_t registry_instance;
	uint64_t handle_serial;
} GDExtensionDeviceExitGuardedLibraryRef;

typedef struct {
	uint64_t registry_instance;
	uint64_t handle_serial;
} GDExtensionDeviceExitInitializationLevelContextRef;

typedef struct {
	uint64_t registry_instance;
	uint64_t handle_serial;
} GDExtensionDeviceExitDeinitLevelContextRef;

typedef struct {
	uint64_t registry_instance;
	uint64_t handle_serial;
} GDExtensionDeviceExitDeinitPreflightContextRef;

typedef struct {
	uint64_t registry_instance;
	uint64_t handle_serial;
} GDExtensionDeviceExitRegisteredCleanupRef;

typedef struct {
	uint64_t registry_instance;
	uint64_t handle_serial;
} GDExtensionDeviceExitLibraryCodeHoldRef;

typedef void (*GDExtensionDeviceExitUserLevelFnV1)(void *binding_userdata, GDExtensionInitializationLevel level);
typedef int32_t (*GDExtensionDeviceExitPreflightFnV1)(GDExtensionDeviceExitDeinitPreflightContextRef context, void *userdata);
typedef struct {
	uint32_t user_services;
	uint32_t binding_services;
} GDExtensionDeviceExitLevelNeedsV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint32_t binding_protocol;
	uint32_t level_bitmap;
	uint32_t user_cleanup_bitmap;
	uint32_t binding_cleanup_bitmap;
	uint64_t entry_classes;
	GDExtensionDeviceExitLevelNeedsV1 needs[4];
	GDExtensionInitialization expected_initialization;
	GDExtensionDeviceExitUserLevelFnV1 user_initializer;
	GDExtensionDeviceExitUserLevelFnV1 user_terminator;
	GDExtensionDeviceExitPreflightFnV1 preflight;
	void *preflight_userdata;
} GDExtensionDeviceExitGuardRegistrationV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	int32_t disposition;
	uint32_t level;
	uint64_t pending_count;
} GDExtensionDeviceExitExitDiagnosticV1;

typedef struct {
	/* registry_instance */
	uint64_t registry_instance;
	/* handle_serial */
	uint64_t handle_serial;
} GDExtensionDeviceRegistryContextRef;

typedef struct {
	/* registry_instance */
	uint64_t registry_instance;
	/* handle_serial */
	uint64_t handle_serial;
} GDExtensionOriginalDeviceSessionRef;

typedef struct {
	/* device_epoch */
	uint64_t device_epoch;
	/* session */
	uint64_t session;
	/* revision */
	uint64_t revision;
	/* state */
	uint32_t state;
	/* new_work */
	uint32_t new_work;
} GDExtensionDeviceSessionViewV1;

typedef struct {
	/* native_error */
	int32_t native_error;
	/* reason */
	uint32_t reason;
	/* expected */
	uint64_t expected;
	/* actual */
	uint64_t actual;
} GDExtensionDeviceSessionResultV1;

typedef struct {
	uint64_t registry_instance;
	uint64_t handle_serial;
} GDExtensionDeviceExitSceneReadyContextRef;

typedef enum {
	GDEXTENSION_SCENE_INSTALL_BASE_V2 = 1,
	GDEXTENSION_SCENE_INSTALL_RD_PAIR_V2 = 2,
} GDExtensionSceneInstallFeatureV2;

typedef enum {
	GDEXTENSION_SCENE_INSTALL_OMISSION_NONE_V2 = 0,
	GDEXTENSION_SCENE_INSTALL_OMISSION_NO_RD_PRESENT_V2 = 1,
} GDExtensionSceneInstallOmissionV2;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint32_t required_features;
	uint32_t optional_features;
	uint64_t reserved;
} GDExtensionSceneInstallRequirementsV2;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t library_generation;
	uint32_t required_features;
	uint32_t optional_features;
	uint32_t selected_features;
	uint32_t omitted_optional_features;
	uint32_t omission_reason;
	uint32_t reserved;
	uint64_t tree_id;
	uint64_t window_id;
	uint64_t world_id;
	uint64_t rendering_server_id;
	uint64_t main_rd_id;
	uint64_t main_rd_epoch;
} GDExtensionSceneInstallPlanV2;

typedef int32_t (*GDExtensionDeviceExitSceneReadyCallbackV2)(void *original_userdata, GDExtensionDeviceExitSceneReadyContextRef original_ready, const GDExtensionSceneInstallPlanV2 *synchronous_plan);
typedef int32_t (*GDExtensionDeviceExitSceneReadyCallbackV1)(void *userdata, GDExtensionDeviceExitSceneReadyContextRef ready);
typedef enum {
	GDEXTENSION_DEVICE_EXIT_ENTRY_SCENE_READY_V1 = 2048,
} GDExtensionDeviceExitSceneReadyEntryClassV1;

typedef enum {
	GDEXTENSION_DEVICE_EXIT_SCENE_READY_ABORT_UNSCHEDULED_PAIR_V1 = 1,
} GDExtensionDeviceExitSceneReadyAbortReasonV1;

typedef struct {
	uint64_t registry_instance;
	uint64_t handle_serial;
} GDExtensionQualificationBridgeRefV1;

typedef struct {
	uint64_t registry_instance;
	uint64_t handle_serial;
} GDExtensionQualificationTicketRefV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t record_generation;
	uint64_t request_nonce;
	uint64_t library_generation;
	uint64_t qualification_session_serial;
	uint64_t range_begin;
	uint64_t range_end;
	uint32_t vector_ordinal;
	uint32_t attempt;
	uint32_t sample_count;
	uint32_t body_groups_x;
	uint8_t request_digest[32];
	uint8_t unit_digest[32];
	uint8_t capture_map_digest[32];
	uint8_t body_map_digest[32];
	uint64_t capture_pipeline;
	uint64_t capture_set;
	uint64_t body_pipeline;
	uint64_t body_set;
	uint64_t readback_buffer;
	uint64_t source_allocation_generation;
	uint64_t observed_backing_serial;
	uint64_t readback_offset;
	uint64_t readback_size;
	uint8_t capture_push[16];
	uint8_t body_push[16];
} GDExtensionQualificationRecordV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t rid;
	uint64_t backing_serial;
	uint64_t byte_size;
	uint64_t usage_bits;
} GDExtensionQualificationBackingV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint32_t state;
	uint32_t phase;
	int32_t native_error;
	uint32_t issue_fact;
	int64_t native_code;
	uint64_t revision;
	uint64_t record_generation;
	uint64_t request_nonce;
	uint32_t required_count;
	uint32_t known_count;
	uint32_t possible_count;
	uint32_t fact_count;
	uint64_t readback_request;
	uint64_t source_backing_serial;
	uint64_t source_offset;
	uint64_t source_size;
	uint32_t owned_size;
	uint32_t reserved;
	uint8_t request_digest[32];
	uint8_t unit_digest[32];
} GDExtensionQualificationReportV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint32_t max_passes;
	uint32_t max_micros;
	uint64_t reserved;
} GDExtensionQualificationBudgetV1;

typedef struct {
	uint64_t graph_instance;
	uint64_t graph_frame;
	uint32_t graph_kind;
	uint32_t graph_index;
	uint32_t graph_offset;
	uint32_t semantic_kind;
	uint32_t command_slot;
	uint32_t backend_queue;
	uint32_t phase;
	uint32_t issue;
	uint32_t completion;
	uint32_t error;
	uint64_t native_serial;
	uint64_t completion_serial;
	int64_t native_code;
	uint32_t parent_fact;
	uint32_t reserved;
	uint64_t source_backing_serial;
	uint64_t source_offset;
	uint64_t source_size;
} GDExtensionQualificationFactV1;

typedef struct {
	uint64_t registry_instance;
	uint64_t handle_serial;
} GDExtensionStorageBackingRefV1;

typedef struct {
	uint64_t registry_instance;
	uint64_t handle_serial;
} GDExtensionStorageRetireRefV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t bytes;
	uint32_t allocation_slot;
	uint32_t part;
	uint64_t allocation_generation;
	uint64_t create_attempt;
	uint64_t reserved;
} GDExtensionStorageCreateRequestV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionStorageBackingRefV1 backing;
	uint64_t rid;
	uint64_t backing_serial;
	uint64_t byte_size;
	uint64_t usage_bits;
	int32_t native_error;
	uint32_t state;
} GDExtensionStorageBackingV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionStorageBackingRefV1 backing;
	uint64_t rid;
	uint64_t observed_backing_serial;
	uint32_t allocation_slot;
	uint32_t part;
	uint64_t allocation_generation;
	uint64_t retire_attempt;
	uint64_t reserved;
} GDExtensionStorageRetireRequestV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint32_t state;
	uint32_t return_status;
	uint64_t rid;
	uint64_t backing_serial;
	uint64_t retire_cell_serial;
	uint32_t frame_slot;
	uint32_t reserved;
	uint64_t frame_epoch;
	uint64_t event_seq;
	uint64_t allocation_generation;
	uint64_t retire_attempt;
	uint32_t allocation_slot;
	uint32_t part;
	uint64_t dependency_set_serial;
	int32_t native_error;
	uint32_t reason;
	int64_t native_code;
} GDExtensionStorageRetireReportV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t record_generation;
	uint64_t request_nonce;
	uint64_t library_generation;
	uint64_t qualification_session_serial;
	uint64_t parent_serial;
	uint64_t reservation_serial;
	uint8_t request_digest[32];
	uint8_t unit_digest[32];
	uint32_t vector_ordinal;
	uint32_t attempt;
	uint32_t upload_count;
	uint32_t max_segments;
	uint32_t expected_result_bytes;
	uint32_t reserved;
} GDExtensionQualificationUploadPrepareV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint32_t upload_index;
	uint32_t role;
	uint32_t owner_kind;
	uint32_t reserved;
	uint64_t rid;
	uint64_t backing_serial;
	uint64_t byte_offset;
	uint64_t byte_size;
	GDExtensionStorageBackingRefV1 e05_backing;
	uint8_t payload_digest[32];
} GDExtensionQualificationUploadPlanV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t record_generation;
	uint64_t parent_serial;
	uint64_t rid;
	uint64_t backing_serial;
	uint64_t byte_offset;
	uint64_t byte_size;
	uint32_t upload_index;
	uint32_t role;
	uint32_t owner_kind;
	uint32_t reserved;
	GDExtensionStorageBackingRefV1 e05_backing;
	uint8_t payload_digest[32];
	uint64_t expected_revision;
} GDExtensionQualificationUploadRequestV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint32_t upload_index;
	uint32_t segment_index;
	uint32_t state;
	uint32_t reserved;
	uint64_t byte_begin;
	uint64_t byte_end;
	uint64_t graph_instance;
	uint64_t graph_frame;
	uint32_t graph_kind;
	uint32_t graph_index;
	uint32_t graph_offset;
	uint32_t reserved2;
	uint64_t backing_serial;
} GDExtensionQualificationUploadSegmentV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint32_t state;
	int32_t native_error;
	uint64_t revision;
	uint64_t record_generation;
	uint64_t parent_serial;
	uint32_t upload_index;
	uint32_t known_count;
	uint32_t possible_count;
	uint32_t out_written;
	uint64_t appended_begin;
	uint64_t appended_end;
	uint64_t possible_begin;
	uint64_t possible_end;
	uint64_t unappended_begin;
	uint64_t unappended_end;
	int64_t native_code;
} GDExtensionQualificationUploadReportV1;

typedef struct {
	uint64_t registry_instance;
	uint64_t handle_serial;
} GDExtensionNativeUnitRefV1;

typedef struct {
	uint64_t registry_instance;
	uint64_t handle_serial;
} GDExtensionNativeUnitRetireRefV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t library_generation;
	uint64_t preparation_generation;
	uint64_t operation_serial;
	uint64_t container_byte_count;
	uint32_t record_slot;
	uint32_t physical_unit;
	uint32_t expected_flags;
	uint32_t reserved32;
	uint8_t container_digest[32];
	uint64_t reserved64;
} GDExtensionNativeUnitCreateRequestV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionNativeUnitRefV1 unit;
	uint64_t shader_rid;
	uint64_t pipeline_rid;
	uint64_t shader_serial;
	uint64_t pipeline_serial;
	uint32_t known_mask;
	uint32_t possible_mask;
	uint32_t state;
	uint32_t effective_flags;
	int32_t native_error;
	uint32_t phase;
	uint64_t event_seq;
} GDExtensionNativeUnitBackingV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionNativeUnitRefV1 unit;
	uint64_t retire_operation;
	uint64_t reserved;
} GDExtensionNativeUnitRetireRequestV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionNativeUnitRefV1 unit;
	uint32_t state;
	uint32_t return_status;
	uint32_t enqueued_mask;
	uint32_t freed_mask;
	uint32_t absent_mask;
	uint32_t unresolved_mask;
	uint32_t shader_frame_slot;
	uint32_t pipeline_frame_slot;
	uint64_t shader_rid;
	uint64_t pipeline_rid;
	uint64_t shader_serial;
	uint64_t pipeline_serial;
	uint64_t shader_frame_epoch;
	uint64_t pipeline_frame_epoch;
	uint64_t shader_free_event_seq;
	uint64_t pipeline_free_event_seq;
	uint64_t dependency_set_serial;
	uint64_t report_revision;
	int32_t native_error;
	uint32_t reason;
	int64_t native_code;
	uint64_t retire_operation;
} GDExtensionNativeUnitRetireReportV1;

typedef enum {
	GDEXTENSION_DEVICE_SESSION_REASON_WRONG_PHASE_V1 = 14,
	GDEXTENSION_DEVICE_SESSION_REASON_WRONG_THREAD_V1 = 15,
	GDEXTENSION_DEVICE_SESSION_REASON_NOT_ADMITTED_V1 = 16,
	GDEXTENSION_DEVICE_SESSION_REASON_STALE_TICKET_V1 = 17,
	GDEXTENSION_DEVICE_SESSION_REASON_NATIVE_UNSUPPORTED_V1 = 18,
	GDEXTENSION_DEVICE_SESSION_REASON_CAPACITY_V1 = 19,
} GDExtensionDeviceSessionQualificationReasonV1;

typedef enum {
	GDEXTENSION_QUALIFICATION_UPLOAD_ROLE_DOMAIN_COUNT_V1 = 0,
	GDEXTENSION_QUALIFICATION_UPLOAD_ROLE_FAULT_PLANE_V1 = 1,
	GDEXTENSION_QUALIFICATION_UPLOAD_ROLE_INPUT_GATE_V1 = 2,
	GDEXTENSION_QUALIFICATION_UPLOAD_ROLE_INSTANCE_GATE_V1 = 3,
	GDEXTENSION_QUALIFICATION_UPLOAD_ROLE_MAPPING_V1 = 4,
	GDEXTENSION_QUALIFICATION_UPLOAD_ROLE_OUTPUT_GATE_V1 = 5,
	GDEXTENSION_QUALIFICATION_UPLOAD_ROLE_RAW_INPUT_V1 = 6,
	GDEXTENSION_QUALIFICATION_UPLOAD_ROLE_RAW_OUTPUT_V1 = 7,
	GDEXTENSION_QUALIFICATION_UPLOAD_ROLE_COUNT_V1 = 8,
} GDExtensionQualificationUploadRoleV1;

typedef enum {
	GDEXTENSION_QUALIFICATION_UPLOAD_OWNER_INVALID_V1 = 0,
	GDEXTENSION_QUALIFICATION_UPLOAD_OWNER_E05_V1 = 1,
	GDEXTENSION_QUALIFICATION_UPLOAD_OWNER_E09_V1 = 2,
	GDEXTENSION_QUALIFICATION_UPLOAD_OWNER_E22_V1 = 3,
	GDEXTENSION_QUALIFICATION_UPLOAD_OWNER_E23_V1 = 4,
	GDEXTENSION_QUALIFICATION_UPLOAD_OWNER_E24_V1 = 5,
} GDExtensionQualificationUploadOwnerKindV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint32_t slot_count;
	uint32_t max_request_bytes;
	uint32_t max_result_bytes;
	uint32_t max_segment_count;
	uint64_t max_payload_bytes;
} GDExtensionQualificationRtConfigV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint32_t kind;
	uint32_t flags;
	uint64_t operation_serial;
	uint64_t payload_bytes;
	uint32_t request_bytes;
	uint32_t result_capacity;
	uint32_t segment_capacity;
	uint32_t reserved;
} GDExtensionQualificationRtRequestV1;

typedef struct {
	uint64_t registry_instance;
	uint64_t handle_serial;
} GDExtensionQualificationRtRefV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint32_t kind;
	uint32_t state;
	uint64_t operation_serial;
	uint64_t slot_generation;
	uint32_t result_bytes;
	uint32_t segment_count;
	GDExtensionDeviceSessionResultV1 call_result;
} GDExtensionQualificationRtObservationV1;

/**
 * @name get_godot_version
 * @since 4.1
 * @deprecated Deprecated in Godot 4.5. Use `get_godot_version2` instead.
 *
 * Gets the Godot version that the GDExtension was loaded into.
 *
 * @param r_godot_version A pointer to the structure to write the version information into.
 */
typedef void (*GDExtensionInterfaceGetGodotVersion)(GDExtensionGodotVersion *r_godot_version);

/**
 * @name get_godot_version2
 * @since 4.5
 *
 * Gets the Godot version that the GDExtension was loaded into.
 *
 * @param r_godot_version A pointer to the structure to write the version information into.
 */
typedef void (*GDExtensionInterfaceGetGodotVersion2)(GDExtensionGodotVersion2 *r_godot_version);

/**
 * @name mem_alloc
 * @since 4.1
 * @deprecated Deprecated in Godot 4.6. Does not allow explicitly requesting padding. Use `mem_alloc2` instead.
 *
 * Allocates memory.
 *
 * @param p_bytes The amount of memory to allocate in bytes.
 *
 * @return A pointer to the allocated memory, or NULL if unsuccessful.
 */
typedef void *(*GDExtensionInterfaceMemAlloc)(size_t p_bytes);

/**
 * @name mem_realloc
 * @since 4.1
 * @deprecated Deprecated in Godot 4.6. Does not allow explicitly requesting padding. Use `mem_realloc2` instead.
 *
 * Reallocates memory.
 *
 * @param p_ptr A pointer to the previously allocated memory.
 * @param p_bytes The number of bytes to resize the memory block to.
 *
 * @return A pointer to the allocated memory, or NULL if unsuccessful.
 */
typedef void *(*GDExtensionInterfaceMemRealloc)(void *p_ptr, size_t p_bytes);

/**
 * @name mem_free
 * @since 4.1
 * @deprecated Deprecated in Godot 4.6. Does not allow explicitly requesting padding. Use `mem_free2` instead.
 *
 * Frees memory.
 *
 * @param p_ptr A pointer to the previously allocated memory.
 */
typedef void (*GDExtensionInterfaceMemFree)(void *p_ptr);

/**
 * @name mem_alloc2
 * @since 4.6
 *
 * Allocates memory.
 *
 * @param p_bytes The amount of memory to allocate in bytes.
 * @param p_pad_align If true, the returned memory will have prepadding of at least 8 bytes.
 *
 * @return A pointer to the allocated memory, or NULL if unsuccessful.
 */
typedef void *(*GDExtensionInterfaceMemAlloc2)(size_t p_bytes, GDExtensionBool p_pad_align);

/**
 * @name mem_realloc2
 * @since 4.6
 *
 * Reallocates memory.
 *
 * @param p_ptr A pointer to the previously allocated memory.
 * @param p_bytes The number of bytes to resize the memory block to.
 * @param p_pad_align If true, the returned memory will have prepadding of at least 8 bytes.
 *
 * @return A pointer to the allocated memory, or NULL if unsuccessful.
 */
typedef void *(*GDExtensionInterfaceMemRealloc2)(void *p_ptr, size_t p_bytes, GDExtensionBool p_pad_align);

/**
 * @name mem_free2
 * @since 4.6
 *
 * Frees memory.
 *
 * @param p_ptr A pointer to the previously allocated memory.
 * @param p_pad_align If true, the given memory was allocated with prepadding.
 */
typedef void (*GDExtensionInterfaceMemFree2)(void *p_ptr, GDExtensionBool p_pad_align);

/**
 * @name print_error
 * @since 4.1
 *
 * Logs an error to Godot's built-in debugger and to the OS terminal.
 *
 * @param p_description The code triggering the error.
 * @param p_function The function name where the error occurred.
 * @param p_file The file where the error occurred.
 * @param p_line The line where the error occurred.
 * @param p_editor_notify Whether or not to notify the editor.
 */
typedef void (*GDExtensionInterfacePrintError)(const char *p_description, const char *p_function, const char *p_file, int32_t p_line, GDExtensionBool p_editor_notify);

/**
 * @name print_error_with_message
 * @since 4.1
 *
 * Logs an error with a message to Godot's built-in debugger and to the OS terminal.
 *
 * @param p_description The code triggering the error.
 * @param p_message The message to show along with the error.
 * @param p_function The function name where the error occurred.
 * @param p_file The file where the error occurred.
 * @param p_line The line where the error occurred.
 * @param p_editor_notify Whether or not to notify the editor.
 */
typedef void (*GDExtensionInterfacePrintErrorWithMessage)(const char *p_description, const char *p_message, const char *p_function, const char *p_file, int32_t p_line, GDExtensionBool p_editor_notify);

/**
 * @name print_warning
 * @since 4.1
 *
 * Logs a warning to Godot's built-in debugger and to the OS terminal.
 *
 * @param p_description The code triggering the warning.
 * @param p_function The function name where the warning occurred.
 * @param p_file The file where the warning occurred.
 * @param p_line The line where the warning occurred.
 * @param p_editor_notify Whether or not to notify the editor.
 */
typedef void (*GDExtensionInterfacePrintWarning)(const char *p_description, const char *p_function, const char *p_file, int32_t p_line, GDExtensionBool p_editor_notify);

/**
 * @name print_warning_with_message
 * @since 4.1
 *
 * Logs a warning with a message to Godot's built-in debugger and to the OS terminal.
 *
 * @param p_description The code triggering the warning.
 * @param p_message The message to show along with the warning.
 * @param p_function The function name where the warning occurred.
 * @param p_file The file where the warning occurred.
 * @param p_line The line where the warning occurred.
 * @param p_editor_notify Whether or not to notify the editor.
 */
typedef void (*GDExtensionInterfacePrintWarningWithMessage)(const char *p_description, const char *p_message, const char *p_function, const char *p_file, int32_t p_line, GDExtensionBool p_editor_notify);

/**
 * @name print_script_error
 * @since 4.1
 *
 * Logs a script error to Godot's built-in debugger and to the OS terminal.
 *
 * @param p_description The code triggering the error.
 * @param p_function The function name where the error occurred.
 * @param p_file The file where the error occurred.
 * @param p_line The line where the error occurred.
 * @param p_editor_notify Whether or not to notify the editor.
 */
typedef void (*GDExtensionInterfacePrintScriptError)(const char *p_description, const char *p_function, const char *p_file, int32_t p_line, GDExtensionBool p_editor_notify);

/**
 * @name print_script_error_with_message
 * @since 4.1
 *
 * Logs a script error with a message to Godot's built-in debugger and to the OS terminal.
 *
 * @param p_description The code triggering the error.
 * @param p_message The message to show along with the error.
 * @param p_function The function name where the error occurred.
 * @param p_file The file where the error occurred.
 * @param p_line The line where the error occurred.
 * @param p_editor_notify Whether or not to notify the editor.
 */
typedef void (*GDExtensionInterfacePrintScriptErrorWithMessage)(const char *p_description, const char *p_message, const char *p_function, const char *p_file, int32_t p_line, GDExtensionBool p_editor_notify);

/**
 * @name get_native_struct_size
 * @since 4.1
 *
 * Gets the size of a native struct (ex. ObjectID) in bytes.
 *
 * @param p_name A pointer to a StringName identifying the struct name.
 *
 * @return The size in bytes.
 */
typedef uint64_t (*GDExtensionInterfaceGetNativeStructSize)(GDExtensionConstStringNamePtr p_name);

/**
 * @name variant_new_copy
 * @since 4.1
 *
 * Copies one Variant into a another.
 *
 * @param r_dest A pointer to the destination Variant.
 * @param p_src A pointer to the source Variant.
 */
typedef void (*GDExtensionInterfaceVariantNewCopy)(GDExtensionUninitializedVariantPtr r_dest, GDExtensionConstVariantPtr p_src);

/**
 * @name variant_new_nil
 * @since 4.1
 *
 * Creates a new Variant containing nil.
 *
 * @param r_dest A pointer to the destination Variant.
 */
typedef void (*GDExtensionInterfaceVariantNewNil)(GDExtensionUninitializedVariantPtr r_dest);

/**
 * @name variant_destroy
 * @since 4.1
 *
 * Destroys a Variant.
 *
 * @param p_self A pointer to the Variant to destroy.
 */
typedef void (*GDExtensionInterfaceVariantDestroy)(GDExtensionVariantPtr p_self);

/**
 * @name variant_call
 * @since 4.1
 *
 * Calls a method on a Variant.
 *
 * @param p_self A pointer to the Variant.
 * @param p_method A pointer to a StringName identifying the method.
 * @param p_args A pointer to a C array of Variant.
 * @param p_argument_count The number of arguments.
 * @param r_return A pointer a Variant which will be assigned the return value.
 * @param r_error A pointer the structure which will hold error information.
 *
 * @see Variant::callp()
 */
typedef void (*GDExtensionInterfaceVariantCall)(GDExtensionVariantPtr p_self, GDExtensionConstStringNamePtr p_method, const GDExtensionConstVariantPtr *p_args, GDExtensionInt p_argument_count, GDExtensionUninitializedVariantPtr r_return, GDExtensionCallError *r_error);

/**
 * @name variant_call_static
 * @since 4.1
 *
 * Calls a static method on a Variant.
 *
 * @param p_type The variant type.
 * @param p_method A pointer to a StringName identifying the method.
 * @param p_args A pointer to a C array of Variant.
 * @param p_argument_count The number of arguments.
 * @param r_return A pointer a Variant which will be assigned the return value.
 * @param r_error A pointer the structure which will be updated with error information.
 *
 * @see Variant::call_static()
 */
typedef void (*GDExtensionInterfaceVariantCallStatic)(GDExtensionVariantType p_type, GDExtensionConstStringNamePtr p_method, const GDExtensionConstVariantPtr *p_args, GDExtensionInt p_argument_count, GDExtensionUninitializedVariantPtr r_return, GDExtensionCallError *r_error);

/**
 * @name variant_evaluate
 * @since 4.1
 *
 * Evaluate an operator on two Variants.
 *
 * @param p_op The operator to evaluate.
 * @param p_a The first Variant.
 * @param p_b The second Variant.
 * @param r_return A pointer a Variant which will be assigned the return value.
 * @param r_valid A pointer to a boolean which will be set to false if the operation is invalid.
 *
 * @see Variant::evaluate()
 */
typedef void (*GDExtensionInterfaceVariantEvaluate)(GDExtensionVariantOperator p_op, GDExtensionConstVariantPtr p_a, GDExtensionConstVariantPtr p_b, GDExtensionUninitializedVariantPtr r_return, GDExtensionBool *r_valid);

/**
 * @name variant_set
 * @since 4.1
 *
 * Sets a key on a Variant to a value.
 *
 * @param p_self A pointer to the Variant.
 * @param p_key A pointer to a Variant representing the key.
 * @param p_value A pointer to a Variant representing the value.
 * @param r_valid A pointer to a boolean which will be set to false if the operation is invalid.
 *
 * @see Variant::set()
 */
typedef void (*GDExtensionInterfaceVariantSet)(GDExtensionVariantPtr p_self, GDExtensionConstVariantPtr p_key, GDExtensionConstVariantPtr p_value, GDExtensionBool *r_valid);

/**
 * @name variant_set_named
 * @since 4.1
 *
 * Sets a named key on a Variant to a value.
 *
 * @param p_self A pointer to the Variant.
 * @param p_key A pointer to a StringName representing the key.
 * @param p_value A pointer to a Variant representing the value.
 * @param r_valid A pointer to a boolean which will be set to false if the operation is invalid.
 *
 * @see Variant::set_named()
 */
typedef void (*GDExtensionInterfaceVariantSetNamed)(GDExtensionVariantPtr p_self, GDExtensionConstStringNamePtr p_key, GDExtensionConstVariantPtr p_value, GDExtensionBool *r_valid);

/**
 * @name variant_set_keyed
 * @since 4.1
 *
 * Sets a keyed property on a Variant to a value.
 *
 * @param p_self A pointer to the Variant.
 * @param p_key A pointer to a Variant representing the key.
 * @param p_value A pointer to a Variant representing the value.
 * @param r_valid A pointer to a boolean which will be set to false if the operation is invalid.
 *
 * @see Variant::set_keyed()
 */
typedef void (*GDExtensionInterfaceVariantSetKeyed)(GDExtensionVariantPtr p_self, GDExtensionConstVariantPtr p_key, GDExtensionConstVariantPtr p_value, GDExtensionBool *r_valid);

/**
 * @name variant_set_indexed
 * @since 4.1
 *
 * Sets an index on a Variant to a value.
 *
 * @param p_self A pointer to the Variant.
 * @param p_index The index.
 * @param p_value A pointer to a Variant representing the value.
 * @param r_valid A pointer to a boolean which will be set to false if the operation is invalid.
 * @param r_oob A pointer to a boolean which will be set to true if the index is out of bounds.
 */
typedef void (*GDExtensionInterfaceVariantSetIndexed)(GDExtensionVariantPtr p_self, GDExtensionInt p_index, GDExtensionConstVariantPtr p_value, GDExtensionBool *r_valid, GDExtensionBool *r_oob);

/**
 * @name variant_get
 * @since 4.1
 *
 * Gets the value of a key from a Variant.
 *
 * @param p_self A pointer to the Variant.
 * @param p_key A pointer to a Variant representing the key.
 * @param r_ret A pointer to a Variant which will be assigned the value.
 * @param r_valid A pointer to a boolean which will be set to false if the operation is invalid.
 */
typedef void (*GDExtensionInterfaceVariantGet)(GDExtensionConstVariantPtr p_self, GDExtensionConstVariantPtr p_key, GDExtensionUninitializedVariantPtr r_ret, GDExtensionBool *r_valid);

/**
 * @name variant_get_named
 * @since 4.1
 *
 * Gets the value of a named key from a Variant.
 *
 * @param p_self A pointer to the Variant.
 * @param p_key A pointer to a StringName representing the key.
 * @param r_ret A pointer to a Variant which will be assigned the value.
 * @param r_valid A pointer to a boolean which will be set to false if the operation is invalid.
 */
typedef void (*GDExtensionInterfaceVariantGetNamed)(GDExtensionConstVariantPtr p_self, GDExtensionConstStringNamePtr p_key, GDExtensionUninitializedVariantPtr r_ret, GDExtensionBool *r_valid);

/**
 * @name variant_get_keyed
 * @since 4.1
 *
 * Gets the value of a keyed property from a Variant.
 *
 * @param p_self A pointer to the Variant.
 * @param p_key A pointer to a Variant representing the key.
 * @param r_ret A pointer to a Variant which will be assigned the value.
 * @param r_valid A pointer to a boolean which will be set to false if the operation is invalid.
 */
typedef void (*GDExtensionInterfaceVariantGetKeyed)(GDExtensionConstVariantPtr p_self, GDExtensionConstVariantPtr p_key, GDExtensionUninitializedVariantPtr r_ret, GDExtensionBool *r_valid);

/**
 * @name variant_get_indexed
 * @since 4.1
 *
 * Gets the value of an index from a Variant.
 *
 * @param p_self A pointer to the Variant.
 * @param p_index The index.
 * @param r_ret A pointer to a Variant which will be assigned the value.
 * @param r_valid A pointer to a boolean which will be set to false if the operation is invalid.
 * @param r_oob A pointer to a boolean which will be set to true if the index is out of bounds.
 */
typedef void (*GDExtensionInterfaceVariantGetIndexed)(GDExtensionConstVariantPtr p_self, GDExtensionInt p_index, GDExtensionUninitializedVariantPtr r_ret, GDExtensionBool *r_valid, GDExtensionBool *r_oob);

/**
 * @name variant_iter_init
 * @since 4.1
 *
 * Initializes an iterator over a Variant.
 *
 * @param p_self A pointer to the Variant.
 * @param r_iter A pointer to a Variant which will be assigned the iterator.
 * @param r_valid A pointer to a boolean which will be set to false if the operation is invalid.
 *
 * @return true if the operation is valid; otherwise false.
 *
 * @see Variant::iter_init()
 */
typedef GDExtensionBool (*GDExtensionInterfaceVariantIterInit)(GDExtensionConstVariantPtr p_self, GDExtensionUninitializedVariantPtr r_iter, GDExtensionBool *r_valid);

/**
 * @name variant_iter_next
 * @since 4.1
 *
 * Gets the next value for an iterator over a Variant.
 *
 * @param p_self A pointer to the Variant.
 * @param r_iter A pointer to a Variant which will be assigned the iterator.
 * @param r_valid A pointer to a boolean which will be set to false if the operation is invalid.
 *
 * @return true if the operation is valid; otherwise false.
 *
 * @see Variant::iter_next()
 */
typedef GDExtensionBool (*GDExtensionInterfaceVariantIterNext)(GDExtensionConstVariantPtr p_self, GDExtensionVariantPtr r_iter, GDExtensionBool *r_valid);

/**
 * @name variant_iter_get
 * @since 4.1
 *
 * Gets the next value for an iterator over a Variant.
 *
 * @param p_self A pointer to the Variant.
 * @param r_iter A pointer to a Variant which will be assigned the iterator.
 * @param r_ret A pointer to a Variant which will be assigned false if the operation is invalid.
 * @param r_valid A pointer to a boolean which will be set to false if the operation is invalid.
 *
 * @see Variant::iter_get()
 */
typedef void (*GDExtensionInterfaceVariantIterGet)(GDExtensionConstVariantPtr p_self, GDExtensionVariantPtr r_iter, GDExtensionUninitializedVariantPtr r_ret, GDExtensionBool *r_valid);

/**
 * @name variant_hash
 * @since 4.1
 *
 * Gets the hash of a Variant.
 *
 * @param p_self A pointer to the Variant.
 *
 * @return The hash value.
 *
 * @see Variant::hash()
 */
typedef GDExtensionInt (*GDExtensionInterfaceVariantHash)(GDExtensionConstVariantPtr p_self);

/**
 * @name variant_recursive_hash
 * @since 4.1
 *
 * Gets the recursive hash of a Variant.
 *
 * @param p_self A pointer to the Variant.
 * @param p_recursion_count The number of recursive loops so far.
 *
 * @return The hash value.
 *
 * @see Variant::recursive_hash()
 */
typedef GDExtensionInt (*GDExtensionInterfaceVariantRecursiveHash)(GDExtensionConstVariantPtr p_self, GDExtensionInt p_recursion_count);

/**
 * @name variant_hash_compare
 * @since 4.1
 *
 * Compares two Variants by their hash.
 *
 * @param p_self A pointer to the Variant.
 * @param p_other A pointer to the other Variant to compare it to.
 *
 * @return The hash value.
 *
 * @see Variant::hash_compare()
 */
typedef GDExtensionBool (*GDExtensionInterfaceVariantHashCompare)(GDExtensionConstVariantPtr p_self, GDExtensionConstVariantPtr p_other);

/**
 * @name variant_booleanize
 * @since 4.1
 *
 * Converts a Variant to a boolean.
 *
 * @param p_self A pointer to the Variant.
 *
 * @return The boolean value of the Variant.
 */
typedef GDExtensionBool (*GDExtensionInterfaceVariantBooleanize)(GDExtensionConstVariantPtr p_self);

/**
 * @name variant_duplicate
 * @since 4.1
 *
 * Duplicates a Variant.
 *
 * @param p_self A pointer to the Variant.
 * @param r_ret A pointer to a Variant to store the duplicated value.
 * @param p_deep Whether or not to duplicate deeply (when supported by the Variant type).
 */
typedef void (*GDExtensionInterfaceVariantDuplicate)(GDExtensionConstVariantPtr p_self, GDExtensionVariantPtr r_ret, GDExtensionBool p_deep);

/**
 * @name variant_stringify
 * @since 4.1
 *
 * Converts a Variant to a string.
 *
 * @param p_self A pointer to the Variant.
 * @param r_ret A pointer to a String to store the resulting value.
 */
typedef void (*GDExtensionInterfaceVariantStringify)(GDExtensionConstVariantPtr p_self, GDExtensionStringPtr r_ret);

/**
 * @name variant_get_type
 * @since 4.1
 *
 * Gets the type of a Variant.
 *
 * @param p_self A pointer to the Variant.
 *
 * @return The variant type.
 */
typedef GDExtensionVariantType (*GDExtensionInterfaceVariantGetType)(GDExtensionConstVariantPtr p_self);

/**
 * @name variant_has_method
 * @since 4.1
 *
 * Checks if a Variant has the given method.
 *
 * @param p_self A pointer to the Variant.
 * @param p_method A pointer to a StringName with the method name.
 *
 * @return true if the variant has the given method; otherwise false.
 */
typedef GDExtensionBool (*GDExtensionInterfaceVariantHasMethod)(GDExtensionConstVariantPtr p_self, GDExtensionConstStringNamePtr p_method);

/**
 * @name variant_has_member
 * @since 4.1
 *
 * Checks if a type of Variant has the given member.
 *
 * @param p_type The Variant type.
 * @param p_member A pointer to a StringName with the member name.
 *
 * @return true if the variant has the given method; otherwise false.
 */
typedef GDExtensionBool (*GDExtensionInterfaceVariantHasMember)(GDExtensionVariantType p_type, GDExtensionConstStringNamePtr p_member);

/**
 * @name variant_has_key
 * @since 4.1
 *
 * Checks if a Variant has a key.
 *
 * @param p_self A pointer to the Variant.
 * @param p_key A pointer to a Variant representing the key.
 * @param r_valid A pointer to a boolean which will be set to false if the key doesn't exist.
 *
 * @return true if the key exists; otherwise false.
 */
typedef GDExtensionBool (*GDExtensionInterfaceVariantHasKey)(GDExtensionConstVariantPtr p_self, GDExtensionConstVariantPtr p_key, GDExtensionBool *r_valid);

/**
 * @name variant_get_object_instance_id
 * @since 4.4
 *
 * Gets the object instance ID from a variant of type GDEXTENSION_VARIANT_TYPE_OBJECT.
 *
 * If the variant isn't of type GDEXTENSION_VARIANT_TYPE_OBJECT, then zero will be returned.
 * The instance ID will be returned even if the object is no longer valid - use `object_get_instance_by_id()` to check if the object is still valid.
 *
 * @param p_self A pointer to the Variant.
 *
 * @return The instance ID for the contained object.
 */
typedef GDObjectInstanceID (*GDExtensionInterfaceVariantGetObjectInstanceId)(GDExtensionConstVariantPtr p_self);

/**
 * @name variant_get_type_name
 * @since 4.1
 *
 * Gets the name of a Variant type.
 *
 * @param p_type The Variant type.
 * @param r_name A pointer to a String to store the Variant type name.
 */
typedef void (*GDExtensionInterfaceVariantGetTypeName)(GDExtensionVariantType p_type, GDExtensionUninitializedStringPtr r_name);

/**
 * @name variant_get_type_by_name
 * @since 4.7
 *
 * Gets the Variant type by name.
 *
 * @param p_type_name The variant type name.
 *
 * @return The variant type for the given name; otherwise VARIANT_MAX if name is invalid.
 */
typedef GDExtensionVariantType (*GDExtensionInterfaceVariantGetTypeByName)(GDExtensionConstStringPtr p_type_name);

/**
 * @name variant_can_convert
 * @since 4.1
 *
 * Checks if Variants can be converted from one type to another.
 *
 * @param p_from The Variant type to convert from.
 * @param p_to The Variant type to convert to.
 *
 * @return true if the conversion is possible; otherwise false.
 */
typedef GDExtensionBool (*GDExtensionInterfaceVariantCanConvert)(GDExtensionVariantType p_from, GDExtensionVariantType p_to);

/**
 * @name variant_can_convert_strict
 * @since 4.1
 *
 * Checks if Variant can be converted from one type to another using stricter rules.
 *
 * @param p_from The Variant type to convert from.
 * @param p_to The Variant type to convert to.
 *
 * @return true if the conversion is possible; otherwise false.
 */
typedef GDExtensionBool (*GDExtensionInterfaceVariantCanConvertStrict)(GDExtensionVariantType p_from, GDExtensionVariantType p_to);

/**
 * @name get_variant_from_type_constructor
 * @since 4.1
 *
 * Gets a pointer to a function that can create a Variant of the given type from a raw value.
 *
 * @param p_type The Variant type.
 *
 * @return A pointer to a function that can create a Variant of the given type from a raw value.
 */
typedef GDExtensionVariantFromTypeConstructorFunc (*GDExtensionInterfaceGetVariantFromTypeConstructor)(GDExtensionVariantType p_type);

/**
 * @name get_variant_to_type_constructor
 * @since 4.1
 *
 * Gets a pointer to a function that can get the raw value from a Variant of the given type.
 *
 * @param p_type The Variant type.
 *
 * @return A pointer to a function that can get the raw value from a Variant of the given type.
 */
typedef GDExtensionTypeFromVariantConstructorFunc (*GDExtensionInterfaceGetVariantToTypeConstructor)(GDExtensionVariantType p_type);

/**
 * @name variant_get_ptr_internal_getter
 * @since 4.4
 *
 * Provides a function pointer for retrieving a pointer to a variant's internal value.
 *
 * Access to a variant's internal value can be used to modify it in-place, or to retrieve its value without the overhead of variant conversion functions.
 * It is recommended to cache the getter for all variant types in a function table to avoid retrieval overhead upon use.
 *
 * Each function assumes the variant's type has already been determined and matches the function.
 * Invoking the function with a variant of a mismatched type has undefined behavior, and may lead to a segmentation fault.
 *
 * @param p_type The Variant type.
 *
 * @return A pointer to a type-specific function that returns a pointer to the internal value of a variant. Check the implementation of this function (gdextension_variant_get_ptr_internal_getter) for pointee type info of each variant type.
 */
typedef GDExtensionVariantGetInternalPtrFunc (*GDExtensionInterfaceVariantGetPtrInternalGetter)(GDExtensionVariantType p_type);

/**
 * @name variant_get_ptr_operator_evaluator
 * @since 4.1
 *
 * Gets a pointer to a function that can evaluate the given Variant operator on the given Variant types.
 *
 * @param p_operator The variant operator.
 * @param p_type_a The type of the first Variant.
 * @param p_type_b The type of the second Variant.
 *
 * @return A pointer to a function that can evaluate the given Variant operator on the given Variant types.
 */
typedef GDExtensionPtrOperatorEvaluator (*GDExtensionInterfaceVariantGetPtrOperatorEvaluator)(GDExtensionVariantOperator p_operator, GDExtensionVariantType p_type_a, GDExtensionVariantType p_type_b);

/**
 * @name variant_get_ptr_builtin_method
 * @since 4.1
 *
 * Gets a pointer to a function that can call a builtin method on a type of Variant.
 *
 * @param p_type The Variant type.
 * @param p_method A pointer to a StringName with the method name.
 * @param p_hash A hash representing the method signature.
 *
 * @return A pointer to a function that can call a builtin method on a type of Variant.
 */
typedef GDExtensionPtrBuiltInMethod (*GDExtensionInterfaceVariantGetPtrBuiltinMethod)(GDExtensionVariantType p_type, GDExtensionConstStringNamePtr p_method, GDExtensionInt p_hash);

/**
 * @name variant_get_ptr_constructor
 * @since 4.1
 *
 * Gets a pointer to a function that can call one of the constructors for a type of Variant.
 *
 * @param p_type The Variant type.
 * @param p_constructor The index of the constructor.
 *
 * @return A pointer to a function that can call one of the constructors for a type of Variant.
 */
typedef GDExtensionPtrConstructor (*GDExtensionInterfaceVariantGetPtrConstructor)(GDExtensionVariantType p_type, int32_t p_constructor);

/**
 * @name variant_get_ptr_destructor
 * @since 4.1
 *
 * Gets a pointer to a function than can call the destructor for a type of Variant.
 *
 * @param p_type The Variant type.
 *
 * @return A pointer to a function than can call the destructor for a type of Variant.
 */
typedef GDExtensionPtrDestructor (*GDExtensionInterfaceVariantGetPtrDestructor)(GDExtensionVariantType p_type);

/**
 * @name variant_construct
 * @since 4.1
 *
 * Constructs a Variant of the given type, using the first constructor that matches the given arguments.
 *
 * @param p_type The Variant type.
 * @param r_base A pointer to a Variant to store the constructed value.
 * @param p_args A pointer to a C array of Variant pointers representing the arguments for the constructor.
 * @param p_argument_count The number of arguments to pass to the constructor.
 * @param r_error A pointer the structure which will be updated with error information.
 */
typedef void (*GDExtensionInterfaceVariantConstruct)(GDExtensionVariantType p_type, GDExtensionUninitializedVariantPtr r_base, const GDExtensionConstVariantPtr *p_args, int32_t p_argument_count, GDExtensionCallError *r_error);

/**
 * @name variant_get_ptr_setter
 * @since 4.1
 *
 * Gets a pointer to a function that can call a member's setter on the given Variant type.
 *
 * @param p_type The Variant type.
 * @param p_member A pointer to a StringName with the member name.
 *
 * @return A pointer to a function that can call a member's setter on the given Variant type.
 */
typedef GDExtensionPtrSetter (*GDExtensionInterfaceVariantGetPtrSetter)(GDExtensionVariantType p_type, GDExtensionConstStringNamePtr p_member);

/**
 * @name variant_get_ptr_getter
 * @since 4.1
 *
 * Gets a pointer to a function that can call a member's getter on the given Variant type.
 *
 * @param p_type The Variant type.
 * @param p_member A pointer to a StringName with the member name.
 *
 * @return A pointer to a function that can call a member's getter on the given Variant type.
 */
typedef GDExtensionPtrGetter (*GDExtensionInterfaceVariantGetPtrGetter)(GDExtensionVariantType p_type, GDExtensionConstStringNamePtr p_member);

/**
 * @name variant_get_ptr_indexed_setter
 * @since 4.1
 *
 * Gets a pointer to a function that can set an index on the given Variant type.
 *
 * @param p_type The Variant type.
 *
 * @return A pointer to a function that can set an index on the given Variant type.
 */
typedef GDExtensionPtrIndexedSetter (*GDExtensionInterfaceVariantGetPtrIndexedSetter)(GDExtensionVariantType p_type);

/**
 * @name variant_get_ptr_indexed_getter
 * @since 4.1
 *
 * Gets a pointer to a function that can get an index on the given Variant type.
 *
 * @param p_type The Variant type.
 *
 * @return A pointer to a function that can get an index on the given Variant type.
 */
typedef GDExtensionPtrIndexedGetter (*GDExtensionInterfaceVariantGetPtrIndexedGetter)(GDExtensionVariantType p_type);

/**
 * @name variant_get_ptr_keyed_setter
 * @since 4.1
 *
 * Gets a pointer to a function that can set a key on the given Variant type.
 *
 * @param p_type The Variant type.
 *
 * @return A pointer to a function that can set a key on the given Variant type.
 */
typedef GDExtensionPtrKeyedSetter (*GDExtensionInterfaceVariantGetPtrKeyedSetter)(GDExtensionVariantType p_type);

/**
 * @name variant_get_ptr_keyed_getter
 * @since 4.1
 *
 * Gets a pointer to a function that can get a key on the given Variant type.
 *
 * @param p_type The Variant type.
 *
 * @return A pointer to a function that can get a key on the given Variant type.
 */
typedef GDExtensionPtrKeyedGetter (*GDExtensionInterfaceVariantGetPtrKeyedGetter)(GDExtensionVariantType p_type);

/**
 * @name variant_get_ptr_keyed_checker
 * @since 4.1
 *
 * Gets a pointer to a function that can check a key on the given Variant type.
 *
 * @param p_type The Variant type.
 *
 * @return A pointer to a function that can check a key on the given Variant type.
 */
typedef GDExtensionPtrKeyedChecker (*GDExtensionInterfaceVariantGetPtrKeyedChecker)(GDExtensionVariantType p_type);

/**
 * @name variant_get_constant_value
 * @since 4.1
 *
 * Gets the value of a constant from the given Variant type.
 *
 * @param p_type The Variant type.
 * @param p_constant A pointer to a StringName with the constant name.
 * @param r_ret A pointer to a Variant to store the value.
 */
typedef void (*GDExtensionInterfaceVariantGetConstantValue)(GDExtensionVariantType p_type, GDExtensionConstStringNamePtr p_constant, GDExtensionUninitializedVariantPtr r_ret);

/**
 * @name variant_get_ptr_utility_function
 * @since 4.1
 *
 * Gets a pointer to a function that can call a Variant utility function.
 *
 * @param p_function A pointer to a StringName with the function name.
 * @param p_hash A hash representing the function signature.
 *
 * @return A pointer to a function that can call a Variant utility function.
 */
typedef GDExtensionPtrUtilityFunction (*GDExtensionInterfaceVariantGetPtrUtilityFunction)(GDExtensionConstStringNamePtr p_function, GDExtensionInt p_hash);

/**
 * @name string_new_with_latin1_chars
 * @since 4.1
 *
 * Creates a String from a Latin-1 encoded C string.
 *
 * @param r_dest A pointer to a Variant to hold the newly created String.
 * @param p_contents A pointer to a Latin-1 encoded C string (null terminated).
 */
typedef void (*GDExtensionInterfaceStringNewWithLatin1Chars)(GDExtensionUninitializedStringPtr r_dest, const char *p_contents);

/**
 * @name string_new_with_utf8_chars
 * @since 4.1
 *
 * Creates a String from a UTF-8 encoded C string.
 *
 * @param r_dest A pointer to a Variant to hold the newly created String.
 * @param p_contents A pointer to a UTF-8 encoded C string (null terminated).
 */
typedef void (*GDExtensionInterfaceStringNewWithUtf8Chars)(GDExtensionUninitializedStringPtr r_dest, const char *p_contents);

/**
 * @name string_new_with_utf16_chars
 * @since 4.1
 *
 * Creates a String from a UTF-16 encoded C string.
 *
 * @param r_dest A pointer to a Variant to hold the newly created String.
 * @param p_contents A pointer to a UTF-16 encoded C string (null terminated).
 */
typedef void (*GDExtensionInterfaceStringNewWithUtf16Chars)(GDExtensionUninitializedStringPtr r_dest, const char16_t *p_contents);

/**
 * @name string_new_with_utf32_chars
 * @since 4.1
 *
 * Creates a String from a UTF-32 encoded C string.
 *
 * @param r_dest A pointer to a Variant to hold the newly created String.
 * @param p_contents A pointer to a UTF-32 encoded C string (null terminated).
 */
typedef void (*GDExtensionInterfaceStringNewWithUtf32Chars)(GDExtensionUninitializedStringPtr r_dest, const char32_t *p_contents);

/**
 * @name string_new_with_wide_chars
 * @since 4.1
 *
 * Creates a String from a wide C string.
 *
 * @param r_dest A pointer to a Variant to hold the newly created String.
 * @param p_contents A pointer to a wide C string (null terminated).
 */
typedef void (*GDExtensionInterfaceStringNewWithWideChars)(GDExtensionUninitializedStringPtr r_dest, const wchar_t *p_contents);

/**
 * @name string_new_with_latin1_chars_and_len
 * @since 4.1
 *
 * Creates a String from a Latin-1 encoded C string with the given length.
 *
 * @param r_dest A pointer to a Variant to hold the newly created String.
 * @param p_contents A pointer to a Latin-1 encoded C string.
 * @param p_size The number of characters (= number of bytes).
 */
typedef void (*GDExtensionInterfaceStringNewWithLatin1CharsAndLen)(GDExtensionUninitializedStringPtr r_dest, const char *p_contents, GDExtensionInt p_size);

/**
 * @name string_new_with_utf8_chars_and_len
 * @since 4.1
 * @deprecated Deprecated in Godot 4.3. Use `string_new_with_utf8_chars_and_len2` instead.
 *
 * Creates a String from a UTF-8 encoded C string with the given length.
 *
 * @param r_dest A pointer to a Variant to hold the newly created String.
 * @param p_contents A pointer to a UTF-8 encoded C string.
 * @param p_size The number of bytes (not code units).
 */
typedef void (*GDExtensionInterfaceStringNewWithUtf8CharsAndLen)(GDExtensionUninitializedStringPtr r_dest, const char *p_contents, GDExtensionInt p_size);

/**
 * @name string_new_with_utf8_chars_and_len2
 * @since 4.3
 *
 * Creates a String from a UTF-8 encoded C string with the given length.
 *
 * @param r_dest A pointer to a Variant to hold the newly created String.
 * @param p_contents A pointer to a UTF-8 encoded C string.
 * @param p_size The number of bytes (not code units).
 *
 * @return Error code signifying if the operation successful.
 */
typedef GDExtensionInt (*GDExtensionInterfaceStringNewWithUtf8CharsAndLen2)(GDExtensionUninitializedStringPtr r_dest, const char *p_contents, GDExtensionInt p_size);

/**
 * @name string_new_with_utf16_chars_and_len
 * @since 4.1
 * @deprecated Deprecated in Godot 4.3. Use `string_new_with_utf16_chars_and_len2` instead.
 *
 * Creates a String from a UTF-16 encoded C string with the given length.
 *
 * @param r_dest A pointer to a Variant to hold the newly created String.
 * @param p_contents A pointer to a UTF-16 encoded C string.
 * @param p_char_count The number of characters (not bytes).
 */
typedef void (*GDExtensionInterfaceStringNewWithUtf16CharsAndLen)(GDExtensionUninitializedStringPtr r_dest, const char16_t *p_contents, GDExtensionInt p_char_count);

/**
 * @name string_new_with_utf16_chars_and_len2
 * @since 4.3
 *
 * Creates a String from a UTF-16 encoded C string with the given length.
 *
 * @param r_dest A pointer to a Variant to hold the newly created String.
 * @param p_contents A pointer to a UTF-16 encoded C string.
 * @param p_char_count The number of characters (not bytes).
 * @param p_default_little_endian If true, UTF-16 use little endian.
 *
 * @return Error code signifying if the operation successful.
 */
typedef GDExtensionInt (*GDExtensionInterfaceStringNewWithUtf16CharsAndLen2)(GDExtensionUninitializedStringPtr r_dest, const char16_t *p_contents, GDExtensionInt p_char_count, GDExtensionBool p_default_little_endian);

/**
 * @name string_new_with_utf32_chars_and_len
 * @since 4.1
 *
 * Creates a String from a UTF-32 encoded C string with the given length.
 *
 * @param r_dest A pointer to a Variant to hold the newly created String.
 * @param p_contents A pointer to a UTF-32 encoded C string.
 * @param p_char_count The number of characters (not bytes).
 */
typedef void (*GDExtensionInterfaceStringNewWithUtf32CharsAndLen)(GDExtensionUninitializedStringPtr r_dest, const char32_t *p_contents, GDExtensionInt p_char_count);

/**
 * @name string_new_with_wide_chars_and_len
 * @since 4.1
 *
 * Creates a String from a wide C string with the given length.
 *
 * @param r_dest A pointer to a Variant to hold the newly created String.
 * @param p_contents A pointer to a wide C string.
 * @param p_char_count The number of characters (not bytes).
 */
typedef void (*GDExtensionInterfaceStringNewWithWideCharsAndLen)(GDExtensionUninitializedStringPtr r_dest, const wchar_t *p_contents, GDExtensionInt p_char_count);

/**
 * @name string_to_latin1_chars
 * @since 4.1
 *
 * Converts a String to a Latin-1 encoded C string.
 *
 * It doesn't write a null terminator.
 *
 * @param p_self A pointer to the String.
 * @param r_text A pointer to the buffer to hold the resulting data. If NULL is passed in, only the length will be computed.
 * @param p_max_write_length The maximum number of characters that can be written to r_text. It has no affect on the return value.
 *
 * @return The resulting encoded string length in characters, not including a null terminator. Characters that cannot be converted to Latin-1 are replaced with a space.
 */
typedef GDExtensionInt (*GDExtensionInterfaceStringToLatin1Chars)(GDExtensionConstStringPtr p_self, char *r_text, GDExtensionInt p_max_write_length);

/**
 * @name string_to_utf8_chars
 * @since 4.1
 *
 * Converts a String to a UTF-8 encoded C string.
 *
 * It doesn't write a null terminator.
 *
 * @param p_self A pointer to the String.
 * @param r_text A pointer to the buffer to hold the resulting data. If NULL is passed in, only the length will be computed.
 * @param p_max_write_length The maximum number of characters that can be written to r_text. It has no affect on the return value.
 *
 * @return The resulting encoded string length in bytes (not characters), not including a null terminator.
 */
typedef GDExtensionInt (*GDExtensionInterfaceStringToUtf8Chars)(GDExtensionConstStringPtr p_self, char *r_text, GDExtensionInt p_max_write_length);

/**
 * @name string_to_utf16_chars
 * @since 4.1
 *
 * Converts a String to a UTF-16 encoded C string.
 *
 * It doesn't write a null terminator.
 *
 * @param p_self A pointer to the String.
 * @param r_text A pointer to the buffer to hold the resulting data. If NULL is passed in, only the length will be computed.
 * @param p_max_write_length The maximum number of characters that can be written to r_text. It has no affect on the return value.
 *
 * @return The resulting encoded string length in 16-bit code units (not bytes or characters), not including a null terminator.
 */
typedef GDExtensionInt (*GDExtensionInterfaceStringToUtf16Chars)(GDExtensionConstStringPtr p_self, char16_t *r_text, GDExtensionInt p_max_write_length);

/**
 * @name string_to_utf32_chars
 * @since 4.1
 *
 * Converts a String to a UTF-32 encoded C string.
 *
 * It doesn't write a null terminator.
 *
 * @param p_self A pointer to the String.
 * @param r_text A pointer to the buffer to hold the resulting data. If NULL is passed in, only the length will be computed.
 * @param p_max_write_length The maximum number of characters that can be written to r_text. It has no affect on the return value.
 *
 * @return The resulting encoded string length in characters (not bytes), not including a null terminator.
 */
typedef GDExtensionInt (*GDExtensionInterfaceStringToUtf32Chars)(GDExtensionConstStringPtr p_self, char32_t *r_text, GDExtensionInt p_max_write_length);

/**
 * @name string_to_wide_chars
 * @since 4.1
 *
 * Converts a String to a wide C string.
 *
 * It doesn't write a null terminator.
 *
 * @param p_self A pointer to the String.
 * @param r_text A pointer to the buffer to hold the resulting data. If NULL is passed in, only the length will be computed.
 * @param p_max_write_length The maximum number of characters that can be written to r_text. It has no affect on the return value.
 *
 * @return The resulting encoded string length in characters (for UTF-32) or 16-bit code units (for UTF-16), depending on the wchar_t representation. Does not include a null terminator.
 */
typedef GDExtensionInt (*GDExtensionInterfaceStringToWideChars)(GDExtensionConstStringPtr p_self, wchar_t *r_text, GDExtensionInt p_max_write_length);

/**
 * @name string_operator_index
 * @since 4.1
 *
 * Gets a pointer to the character at the given index from a String.
 *
 * @param p_self A pointer to the String.
 * @param p_index The index.
 *
 * @return A pointer to the requested character.
 */
typedef char32_t *(*GDExtensionInterfaceStringOperatorIndex)(GDExtensionStringPtr p_self, GDExtensionInt p_index);

/**
 * @name string_operator_index_const
 * @since 4.1
 *
 * Gets a const pointer to the character at the given index from a String.
 *
 * @param p_self A pointer to the String.
 * @param p_index The index.
 *
 * @return A const pointer to the requested character.
 */
typedef const char32_t *(*GDExtensionInterfaceStringOperatorIndexConst)(GDExtensionConstStringPtr p_self, GDExtensionInt p_index);

/**
 * @name string_operator_plus_eq_string
 * @since 4.1
 *
 * Appends another String to a String.
 *
 * @param p_self A pointer to the String.
 * @param p_b A pointer to the other String to append.
 */
typedef void (*GDExtensionInterfaceStringOperatorPlusEqString)(GDExtensionStringPtr p_self, GDExtensionConstStringPtr p_b);

/**
 * @name string_operator_plus_eq_char
 * @since 4.1
 *
 * Appends a character to a String.
 *
 * @param p_self A pointer to the String.
 * @param p_b A pointer to the character to append.
 */
typedef void (*GDExtensionInterfaceStringOperatorPlusEqChar)(GDExtensionStringPtr p_self, char32_t p_b);

/**
 * @name string_operator_plus_eq_cstr
 * @since 4.1
 *
 * Appends a Latin-1 encoded C string to a String.
 *
 * @param p_self A pointer to the String.
 * @param p_b A pointer to a Latin-1 encoded C string (null terminated).
 */
typedef void (*GDExtensionInterfaceStringOperatorPlusEqCstr)(GDExtensionStringPtr p_self, const char *p_b);

/**
 * @name string_operator_plus_eq_wcstr
 * @since 4.1
 *
 * Appends a wide C string to a String.
 *
 * @param p_self A pointer to the String.
 * @param p_b A pointer to a wide C string (null terminated).
 */
typedef void (*GDExtensionInterfaceStringOperatorPlusEqWcstr)(GDExtensionStringPtr p_self, const wchar_t *p_b);

/**
 * @name string_operator_plus_eq_c32str
 * @since 4.1
 *
 * Appends a UTF-32 encoded C string to a String.
 *
 * @param p_self A pointer to the String.
 * @param p_b A pointer to a UTF-32 encoded C string (null terminated).
 */
typedef void (*GDExtensionInterfaceStringOperatorPlusEqC32str)(GDExtensionStringPtr p_self, const char32_t *p_b);

/**
 * @name string_resize
 * @since 4.2
 *
 * Resizes the underlying string data to the given number of characters.
 *
 * Space needs to be allocated for the null terminating character ('\0') which
 * also must be added manually, in order for all string functions to work correctly.
 *
 * Warning: This is an error-prone operation - only use it if there's no other
 * efficient way to accomplish your goal.
 *
 * @param p_self A pointer to the String.
 * @param p_resize The new length for the String.
 *
 * @return Error code signifying if the operation successful.
 */
typedef GDExtensionInt (*GDExtensionInterfaceStringResize)(GDExtensionStringPtr p_self, GDExtensionInt p_resize);

/**
 * @name string_name_new_with_latin1_chars
 * @since 4.2
 *
 * Creates a StringName from a Latin-1 encoded C string.
 *
 * If `p_is_static` is true, then:
 * - The StringName will reuse the `p_contents` buffer instead of copying it.
 * - You must guarantee that the buffer remains valid for the duration of the application (e.g. string literal).
 * - You must not call a destructor for this StringName. Incrementing the initial reference once should achieve this.
 *
 * `p_is_static` is purely an optimization and can easily introduce undefined behavior if used wrong. In case of doubt, set it to false.
 *
 * @param r_dest A pointer to uninitialized storage, into which the newly created StringName is constructed.
 * @param p_contents A pointer to a C string (null terminated and Latin-1 or ASCII encoded).
 * @param p_is_static Whether the StringName reuses the buffer directly (see above).
 */
typedef void (*GDExtensionInterfaceStringNameNewWithLatin1Chars)(GDExtensionUninitializedStringNamePtr r_dest, const char *p_contents, GDExtensionBool p_is_static);

/**
 * @name string_name_new_with_utf8_chars
 * @since 4.2
 *
 * Creates a StringName from a UTF-8 encoded C string.
 *
 * @param r_dest A pointer to uninitialized storage, into which the newly created StringName is constructed.
 * @param p_contents A pointer to a C string (null terminated and UTF-8 encoded).
 */
typedef void (*GDExtensionInterfaceStringNameNewWithUtf8Chars)(GDExtensionUninitializedStringNamePtr r_dest, const char *p_contents);

/**
 * @name string_name_new_with_utf8_chars_and_len
 * @since 4.2
 *
 * Creates a StringName from a UTF-8 encoded string with a given number of characters.
 *
 * @param r_dest A pointer to uninitialized storage, into which the newly created StringName is constructed.
 * @param p_contents A pointer to a C string (null terminated and UTF-8 encoded).
 * @param p_size The number of bytes (not UTF-8 code points).
 */
typedef void (*GDExtensionInterfaceStringNameNewWithUtf8CharsAndLen)(GDExtensionUninitializedStringNamePtr r_dest, const char *p_contents, GDExtensionInt p_size);

/**
 * @name xml_parser_open_buffer
 * @since 4.1
 *
 * Opens a raw XML buffer on an XMLParser instance.
 *
 * @param p_instance A pointer to an XMLParser object.
 * @param p_buffer A pointer to the buffer.
 * @param p_size The size of the buffer.
 *
 * @return A Godot error code (ex. OK, ERR_INVALID_DATA, etc).
 *
 * @see XMLParser::open_buffer()
 */
typedef GDExtensionInt (*GDExtensionInterfaceXmlParserOpenBuffer)(GDExtensionObjectPtr p_instance, const uint8_t *p_buffer, size_t p_size);

/**
 * @name file_access_store_buffer
 * @since 4.1
 *
 * Stores the given buffer using an instance of FileAccess.
 *
 * @param p_instance A pointer to a FileAccess object.
 * @param p_src A pointer to the buffer.
 * @param p_length The size of the buffer.
 *
 * @see FileAccess::store_buffer()
 */
typedef void (*GDExtensionInterfaceFileAccessStoreBuffer)(GDExtensionObjectPtr p_instance, const uint8_t *p_src, uint64_t p_length);

/**
 * @name file_access_get_buffer
 * @since 4.1
 *
 * Reads the next p_length bytes into the given buffer using an instance of FileAccess.
 *
 * @param p_instance A pointer to a FileAccess object.
 * @param p_dst A pointer to the buffer to store the data.
 * @param p_length The requested number of bytes to read.
 *
 * @return The actual number of bytes read (may be less than requested).
 */
typedef uint64_t (*GDExtensionInterfaceFileAccessGetBuffer)(GDExtensionConstObjectPtr p_instance, uint8_t *p_dst, uint64_t p_length);

/**
 * @name image_ptrw
 * @since 4.3
 *
 * Returns writable pointer to internal Image buffer.
 *
 * @param p_instance A pointer to a Image object.
 *
 * @return Pointer to internal Image buffer.
 *
 * @see Image::ptrw()
 */
typedef uint8_t *(*GDExtensionInterfaceImagePtrw)(GDExtensionObjectPtr p_instance);

/**
 * @name image_ptr
 * @since 4.3
 *
 * Returns read only pointer to internal Image buffer.
 *
 * @param p_instance A pointer to a Image object.
 *
 * @return Pointer to internal Image buffer.
 *
 * @see Image::ptr()
 */
typedef const uint8_t *(*GDExtensionInterfaceImagePtr)(GDExtensionObjectPtr p_instance);

/**
 * @name worker_thread_pool_add_native_group_task
 * @since 4.1
 *
 * Adds a group task to an instance of WorkerThreadPool.
 *
 * @param p_instance A pointer to a WorkerThreadPool object.
 * @param p_func A pointer to a function to run in the thread pool.
 * @param p_userdata A pointer to arbitrary data which will be passed to p_func.
 * @param p_elements The number of element needed in the group.
 * @param p_tasks The number of tasks needed in the group.
 * @param p_high_priority Whether or not this is a high priority task.
 * @param p_description A pointer to a String with the task description.
 *
 * @return The task group ID.
 *
 * @see WorkerThreadPool::add_group_task()
 */
typedef int64_t (*GDExtensionInterfaceWorkerThreadPoolAddNativeGroupTask)(GDExtensionObjectPtr p_instance, GDExtensionWorkerThreadPoolGroupTask p_func, void *p_userdata, int32_t p_elements, int32_t p_tasks, GDExtensionBool p_high_priority, GDExtensionConstStringPtr p_description);

/**
 * @name worker_thread_pool_add_native_task
 * @since 4.1
 *
 * Adds a task to an instance of WorkerThreadPool.
 *
 * @param p_instance A pointer to a WorkerThreadPool object.
 * @param p_func A pointer to a function to run in the thread pool.
 * @param p_userdata A pointer to arbitrary data which will be passed to p_func.
 * @param p_high_priority Whether or not this is a high priority task.
 * @param p_description A pointer to a String with the task description.
 *
 * @return The task ID.
 */
typedef int64_t (*GDExtensionInterfaceWorkerThreadPoolAddNativeTask)(GDExtensionObjectPtr p_instance, GDExtensionWorkerThreadPoolTask p_func, void *p_userdata, GDExtensionBool p_high_priority, GDExtensionConstStringPtr p_description);

/**
 * @name packed_byte_array_operator_index
 * @since 4.1
 *
 * Gets a pointer to a byte in a PackedByteArray.
 *
 * @param p_self A pointer to a PackedByteArray object.
 * @param p_index The index of the byte to get.
 *
 * @return A pointer to the requested byte.
 */
typedef uint8_t *(*GDExtensionInterfacePackedByteArrayOperatorIndex)(GDExtensionTypePtr p_self, GDExtensionInt p_index);

/**
 * @name packed_byte_array_operator_index_const
 * @since 4.1
 *
 * Gets a const pointer to a byte in a PackedByteArray.
 *
 * @param p_self A const pointer to a PackedByteArray object.
 * @param p_index The index of the byte to get.
 *
 * @return A const pointer to the requested byte.
 */
typedef const uint8_t *(*GDExtensionInterfacePackedByteArrayOperatorIndexConst)(GDExtensionConstTypePtr p_self, GDExtensionInt p_index);

/**
 * @name packed_float32_array_operator_index
 * @since 4.1
 *
 * Gets a pointer to a 32-bit float in a PackedFloat32Array.
 *
 * @param p_self A pointer to a PackedFloat32Array object.
 * @param p_index The index of the float to get.
 *
 * @return A pointer to the requested 32-bit float.
 */
typedef float *(*GDExtensionInterfacePackedFloat32ArrayOperatorIndex)(GDExtensionTypePtr p_self, GDExtensionInt p_index);

/**
 * @name packed_float32_array_operator_index_const
 * @since 4.1
 *
 * Gets a const pointer to a 32-bit float in a PackedFloat32Array.
 *
 * @param p_self A const pointer to a PackedFloat32Array object.
 * @param p_index The index of the float to get.
 *
 * @return A const pointer to the requested 32-bit float.
 */
typedef const float *(*GDExtensionInterfacePackedFloat32ArrayOperatorIndexConst)(GDExtensionConstTypePtr p_self, GDExtensionInt p_index);

/**
 * @name packed_float64_array_operator_index
 * @since 4.1
 *
 * Gets a pointer to a 64-bit float in a PackedFloat64Array.
 *
 * @param p_self A pointer to a PackedFloat64Array object.
 * @param p_index The index of the float to get.
 *
 * @return A pointer to the requested 64-bit float.
 */
typedef double *(*GDExtensionInterfacePackedFloat64ArrayOperatorIndex)(GDExtensionTypePtr p_self, GDExtensionInt p_index);

/**
 * @name packed_float64_array_operator_index_const
 * @since 4.1
 *
 * Gets a const pointer to a 64-bit float in a PackedFloat64Array.
 *
 * @param p_self A const pointer to a PackedFloat64Array object.
 * @param p_index The index of the float to get.
 *
 * @return A const pointer to the requested 64-bit float.
 */
typedef const double *(*GDExtensionInterfacePackedFloat64ArrayOperatorIndexConst)(GDExtensionConstTypePtr p_self, GDExtensionInt p_index);

/**
 * @name packed_int32_array_operator_index
 * @since 4.1
 *
 * Gets a pointer to a 32-bit integer in a PackedInt32Array.
 *
 * @param p_self A pointer to a PackedInt32Array object.
 * @param p_index The index of the integer to get.
 *
 * @return A pointer to the requested 32-bit integer.
 */
typedef int32_t *(*GDExtensionInterfacePackedInt32ArrayOperatorIndex)(GDExtensionTypePtr p_self, GDExtensionInt p_index);

/**
 * @name packed_int32_array_operator_index_const
 * @since 4.1
 *
 * Gets a const pointer to a 32-bit integer in a PackedInt32Array.
 *
 * @param p_self A const pointer to a PackedInt32Array object.
 * @param p_index The index of the integer to get.
 *
 * @return A const pointer to the requested 32-bit integer.
 */
typedef const int32_t *(*GDExtensionInterfacePackedInt32ArrayOperatorIndexConst)(GDExtensionConstTypePtr p_self, GDExtensionInt p_index);

/**
 * @name packed_int64_array_operator_index
 * @since 4.1
 *
 * Gets a pointer to a 64-bit integer in a PackedInt64Array.
 *
 * @param p_self A pointer to a PackedInt64Array object.
 * @param p_index The index of the integer to get.
 *
 * @return A pointer to the requested 64-bit integer.
 */
typedef int64_t *(*GDExtensionInterfacePackedInt64ArrayOperatorIndex)(GDExtensionTypePtr p_self, GDExtensionInt p_index);

/**
 * @name packed_int64_array_operator_index_const
 * @since 4.1
 *
 * Gets a const pointer to a 64-bit integer in a PackedInt64Array.
 *
 * @param p_self A const pointer to a PackedInt64Array object.
 * @param p_index The index of the integer to get.
 *
 * @return A const pointer to the requested 64-bit integer.
 */
typedef const int64_t *(*GDExtensionInterfacePackedInt64ArrayOperatorIndexConst)(GDExtensionConstTypePtr p_self, GDExtensionInt p_index);

/**
 * @name packed_string_array_operator_index
 * @since 4.1
 *
 * Gets a pointer to a string in a PackedStringArray.
 *
 * @param p_self A pointer to a PackedStringArray object.
 * @param p_index The index of the String to get.
 *
 * @return A pointer to the requested String.
 */
typedef GDExtensionStringPtr (*GDExtensionInterfacePackedStringArrayOperatorIndex)(GDExtensionTypePtr p_self, GDExtensionInt p_index);

/**
 * @name packed_string_array_operator_index_const
 * @since 4.1
 *
 * Gets a const pointer to a string in a PackedStringArray.
 *
 * @param p_self A const pointer to a PackedStringArray object.
 * @param p_index The index of the String to get.
 *
 * @return A const pointer to the requested String.
 */
typedef GDExtensionStringPtr (*GDExtensionInterfacePackedStringArrayOperatorIndexConst)(GDExtensionConstTypePtr p_self, GDExtensionInt p_index);

/**
 * @name packed_vector2_array_operator_index
 * @since 4.1
 *
 * Gets a pointer to a Vector2 in a PackedVector2Array.
 *
 * @param p_self A pointer to a PackedVector2Array object.
 * @param p_index The index of the Vector2 to get.
 *
 * @return A pointer to the requested Vector2.
 */
typedef GDExtensionTypePtr (*GDExtensionInterfacePackedVector2ArrayOperatorIndex)(GDExtensionTypePtr p_self, GDExtensionInt p_index);

/**
 * @name packed_vector2_array_operator_index_const
 * @since 4.1
 *
 * Gets a const pointer to a Vector2 in a PackedVector2Array.
 *
 * @param p_self A const pointer to a PackedVector2Array object.
 * @param p_index The index of the Vector2 to get.
 *
 * @return A const pointer to the requested Vector2.
 */
typedef GDExtensionTypePtr (*GDExtensionInterfacePackedVector2ArrayOperatorIndexConst)(GDExtensionConstTypePtr p_self, GDExtensionInt p_index);

/**
 * @name packed_vector3_array_operator_index
 * @since 4.1
 *
 * Gets a pointer to a Vector3 in a PackedVector3Array.
 *
 * @param p_self A pointer to a PackedVector3Array object.
 * @param p_index The index of the Vector3 to get.
 *
 * @return A pointer to the requested Vector3.
 */
typedef GDExtensionTypePtr (*GDExtensionInterfacePackedVector3ArrayOperatorIndex)(GDExtensionTypePtr p_self, GDExtensionInt p_index);

/**
 * @name packed_vector3_array_operator_index_const
 * @since 4.1
 *
 * Gets a const pointer to a Vector3 in a PackedVector3Array.
 *
 * @param p_self A const pointer to a PackedVector3Array object.
 * @param p_index The index of the Vector3 to get.
 *
 * @return A const pointer to the requested Vector3.
 */
typedef GDExtensionTypePtr (*GDExtensionInterfacePackedVector3ArrayOperatorIndexConst)(GDExtensionConstTypePtr p_self, GDExtensionInt p_index);

/**
 * @name packed_vector4_array_operator_index
 * @since 4.3
 *
 * Gets a pointer to a Vector4 in a PackedVector4Array.
 *
 * @param p_self A pointer to a PackedVector4Array object.
 * @param p_index The index of the Vector4 to get.
 *
 * @return A pointer to the requested Vector4.
 */
typedef GDExtensionTypePtr (*GDExtensionInterfacePackedVector4ArrayOperatorIndex)(GDExtensionTypePtr p_self, GDExtensionInt p_index);

/**
 * @name packed_vector4_array_operator_index_const
 * @since 4.3
 *
 * Gets a const pointer to a Vector4 in a PackedVector4Array.
 *
 * @param p_self A const pointer to a PackedVector4Array object.
 * @param p_index The index of the Vector4 to get.
 *
 * @return A const pointer to the requested Vector4.
 */
typedef GDExtensionTypePtr (*GDExtensionInterfacePackedVector4ArrayOperatorIndexConst)(GDExtensionConstTypePtr p_self, GDExtensionInt p_index);

/**
 * @name packed_color_array_operator_index
 * @since 4.1
 *
 * Gets a pointer to a color in a PackedColorArray.
 *
 * @param p_self A pointer to a PackedColorArray object.
 * @param p_index The index of the Color to get.
 *
 * @return A pointer to the requested Color.
 */
typedef GDExtensionTypePtr (*GDExtensionInterfacePackedColorArrayOperatorIndex)(GDExtensionTypePtr p_self, GDExtensionInt p_index);

/**
 * @name packed_color_array_operator_index_const
 * @since 4.1
 *
 * Gets a const pointer to a color in a PackedColorArray.
 *
 * @param p_self A const pointer to a PackedColorArray object.
 * @param p_index The index of the Color to get.
 *
 * @return A const pointer to the requested Color.
 */
typedef GDExtensionTypePtr (*GDExtensionInterfacePackedColorArrayOperatorIndexConst)(GDExtensionConstTypePtr p_self, GDExtensionInt p_index);

/**
 * @name array_operator_index
 * @since 4.1
 *
 * Gets a pointer to a Variant in an Array.
 *
 * @param p_self A pointer to an Array object.
 * @param p_index The index of the Variant to get.
 *
 * @return A pointer to the requested Variant.
 */
typedef GDExtensionVariantPtr (*GDExtensionInterfaceArrayOperatorIndex)(GDExtensionTypePtr p_self, GDExtensionInt p_index);

/**
 * @name array_operator_index_const
 * @since 4.1
 *
 * Gets a const pointer to a Variant in an Array.
 *
 * @param p_self A const pointer to an Array object.
 * @param p_index The index of the Variant to get.
 *
 * @return A const pointer to the requested Variant.
 */
typedef GDExtensionVariantPtr (*GDExtensionInterfaceArrayOperatorIndexConst)(GDExtensionConstTypePtr p_self, GDExtensionInt p_index);

/**
 * @name array_ref
 * @since 4.1
 * @deprecated Deprecated in Godot 4.5. Removed from interface. Use copy constructor instead.
 *
 * Sets an Array to be a reference to another Array object.
 *
 * @param p_self A pointer to the Array object to update.
 * @param p_from A pointer to the Array object to reference.
 */
typedef void (*GDExtensionInterfaceArrayRef)(GDExtensionTypePtr p_self, GDExtensionConstTypePtr p_from);

/**
 * @name array_set_typed
 * @since 4.1
 *
 * Makes an Array into a typed Array.
 *
 * @param p_self A pointer to the Array.
 * @param p_type The type of Variant the Array will store.
 * @param p_class_name A pointer to a StringName with the name of the object (if p_type is GDEXTENSION_VARIANT_TYPE_OBJECT).
 * @param p_script A pointer to a Script object (if p_type is GDEXTENSION_VARIANT_TYPE_OBJECT and the base class is extended by a script).
 */
typedef void (*GDExtensionInterfaceArraySetTyped)(GDExtensionTypePtr p_self, GDExtensionVariantType p_type, GDExtensionConstStringNamePtr p_class_name, GDExtensionConstVariantPtr p_script);

/**
 * @name dictionary_operator_index
 * @since 4.1
 *
 * Gets a pointer to a Variant in a Dictionary with the given key.
 *
 * @param p_self A pointer to a Dictionary object.
 * @param p_key A pointer to a Variant representing the key.
 *
 * @return A pointer to a Variant representing the value at the given key.
 */
typedef GDExtensionVariantPtr (*GDExtensionInterfaceDictionaryOperatorIndex)(GDExtensionTypePtr p_self, GDExtensionConstVariantPtr p_key);

/**
 * @name dictionary_operator_index_const
 * @since 4.1
 *
 * Gets a const pointer to a Variant in a Dictionary with the given key.
 *
 * @param p_self A const pointer to a Dictionary object.
 * @param p_key A pointer to a Variant representing the key.
 *
 * @return A const pointer to a Variant representing the value at the given key.
 */
typedef GDExtensionVariantPtr (*GDExtensionInterfaceDictionaryOperatorIndexConst)(GDExtensionConstTypePtr p_self, GDExtensionConstVariantPtr p_key);

/**
 * @name dictionary_set_typed
 * @since 4.4
 *
 * Makes a Dictionary into a typed Dictionary.
 *
 * @param p_self A pointer to the Dictionary.
 * @param p_key_type The type of Variant the Dictionary key will store.
 * @param p_key_class_name A pointer to a StringName with the name of the object (if p_key_type is GDEXTENSION_VARIANT_TYPE_OBJECT).
 * @param p_key_script A pointer to a Script object (if p_key_type is GDEXTENSION_VARIANT_TYPE_OBJECT and the base class is extended by a script).
 * @param p_value_type The type of Variant the Dictionary value will store.
 * @param p_value_class_name A pointer to a StringName with the name of the object (if p_value_type is GDEXTENSION_VARIANT_TYPE_OBJECT).
 * @param p_value_script A pointer to a Script object (if p_value_type is GDEXTENSION_VARIANT_TYPE_OBJECT and the base class is extended by a script).
 */
typedef void (*GDExtensionInterfaceDictionarySetTyped)(GDExtensionTypePtr p_self, GDExtensionVariantType p_key_type, GDExtensionConstStringNamePtr p_key_class_name, GDExtensionConstVariantPtr p_key_script, GDExtensionVariantType p_value_type, GDExtensionConstStringNamePtr p_value_class_name, GDExtensionConstVariantPtr p_value_script);

/**
 * @name object_method_bind_call
 * @since 4.1
 *
 * Calls a method on an Object.
 *
 * @param p_method_bind A pointer to the MethodBind representing the method on the Object's class.
 * @param p_instance A pointer to the Object.
 * @param p_args A pointer to a C array of Variants representing the arguments.
 * @param p_arg_count The number of arguments.
 * @param r_ret A pointer to Variant which will receive the return value.
 * @param r_error A pointer to a GDExtensionCallError struct that will receive error information.
 */
typedef void (*GDExtensionInterfaceObjectMethodBindCall)(GDExtensionMethodBindPtr p_method_bind, GDExtensionObjectPtr p_instance, const GDExtensionConstVariantPtr *p_args, GDExtensionInt p_arg_count, GDExtensionUninitializedVariantPtr r_ret, GDExtensionCallError *r_error);

/**
 * @name object_method_bind_ptrcall
 * @since 4.1
 *
 * Calls a method on an Object (using a "ptrcall").
 *
 * @param p_method_bind A pointer to the MethodBind representing the method on the Object's class.
 * @param p_instance A pointer to the Object.
 * @param p_args A pointer to a C array representing the arguments.
 * @param r_ret A pointer to the Object that will receive the return value.
 */
typedef void (*GDExtensionInterfaceObjectMethodBindPtrcall)(GDExtensionMethodBindPtr p_method_bind, GDExtensionObjectPtr p_instance, const GDExtensionConstTypePtr *p_args, GDExtensionTypePtr r_ret);

/**
 * @name object_destroy
 * @since 4.1
 *
 * Destroys an Object.
 *
 * @param p_o A pointer to the Object.
 */
typedef void (*GDExtensionInterfaceObjectDestroy)(GDExtensionObjectPtr p_o);

/**
 * @name global_get_singleton
 * @since 4.1
 *
 * Gets a global singleton by name.
 *
 * @param p_name A pointer to a StringName with the singleton name.
 *
 * @return A pointer to the singleton Object.
 */
typedef GDExtensionObjectPtr (*GDExtensionInterfaceGlobalGetSingleton)(GDExtensionConstStringNamePtr p_name);

/**
 * @name object_get_instance_binding
 * @since 4.1
 *
 * Gets a pointer representing an Object's instance binding.
 *
 * @param p_o A pointer to the Object.
 * @param p_token A token the library received by the GDExtension's entry point function.
 * @param p_callbacks A pointer to a GDExtensionInstanceBindingCallbacks struct.
 *
 * @return A pointer to the instance binding.
 */
typedef void *(*GDExtensionInterfaceObjectGetInstanceBinding)(GDExtensionObjectPtr p_o, void *p_token, const GDExtensionInstanceBindingCallbacks *p_callbacks);

/**
 * @name object_set_instance_binding
 * @since 4.1
 *
 * Sets an Object's instance binding.
 *
 * @param p_o A pointer to the Object.
 * @param p_token A token the library received by the GDExtension's entry point function.
 * @param p_binding A pointer to the instance binding.
 * @param p_callbacks A pointer to a GDExtensionInstanceBindingCallbacks struct.
 */
typedef void (*GDExtensionInterfaceObjectSetInstanceBinding)(GDExtensionObjectPtr p_o, void *p_token, void *p_binding, const GDExtensionInstanceBindingCallbacks *p_callbacks);

/**
 * @name object_free_instance_binding
 * @since 4.2
 *
 * Free an Object's instance binding.
 *
 * @param p_o A pointer to the Object.
 * @param p_token A token the library received by the GDExtension's entry point function.
 */
typedef void (*GDExtensionInterfaceObjectFreeInstanceBinding)(GDExtensionObjectPtr p_o, void *p_token);

/**
 * @name object_set_instance
 * @since 4.1
 *
 * Sets an extension class instance on a Object.
 *
 * `p_classname` should be a registered extension class and should extend the `p_o` Object's class.
 *
 * @param p_o A pointer to the Object.
 * @param p_classname A pointer to a StringName with the registered extension class's name.
 * @param p_instance A pointer to the extension class instance.
 */
typedef void (*GDExtensionInterfaceObjectSetInstance)(GDExtensionObjectPtr p_o, GDExtensionConstStringNamePtr p_classname, GDExtensionClassInstancePtr p_instance);

/**
 * @name object_get_class_name
 * @since 4.1
 *
 * Gets the class name of an Object.
 *
 * If the GDExtension wraps the Godot object in an abstraction specific to its class, this is the
 * function that should be used to determine which wrapper to use.
 *
 * @param p_object A pointer to the Object.
 * @param p_library A pointer the library received by the GDExtension's entry point function.
 * @param r_class_name A pointer to a String to receive the class name.
 *
 * @return true if successful in getting the class name; otherwise false.
 */
typedef GDExtensionBool (*GDExtensionInterfaceObjectGetClassName)(GDExtensionConstObjectPtr p_object, GDExtensionClassLibraryPtr p_library, GDExtensionUninitializedStringNamePtr r_class_name);

/**
 * @name object_cast_to
 * @since 4.1
 * @deprecated Deprecated in Godot 4.7. Use the `is_class` method on `Object` to check if an object can be cast instead. If true, the previous pointer can be reinterpreted as a pointer to the target type.
 *
 * Casts an Object to a different type.
 *
 * @param p_object A pointer to the Object.
 * @param p_class_tag A pointer uniquely identifying a built-in class in the ClassDB.
 *
 * @return Returns a pointer to the Object, or NULL if it can't be cast to the requested type.
 */
typedef GDExtensionObjectPtr (*GDExtensionInterfaceObjectCastTo)(GDExtensionConstObjectPtr p_object, void *p_class_tag);

/**
 * @name object_get_instance_from_id
 * @since 4.1
 *
 * Gets an Object by its instance ID.
 *
 * @param p_instance_id The instance ID.
 *
 * @return A pointer to the Object.
 */
typedef GDExtensionObjectPtr (*GDExtensionInterfaceObjectGetInstanceFromId)(GDObjectInstanceID p_instance_id);

/**
 * @name object_get_instance_id
 * @since 4.1
 *
 * Gets the instance ID from an Object.
 *
 * @param p_object A pointer to the Object.
 *
 * @return The instance ID.
 */
typedef GDObjectInstanceID (*GDExtensionInterfaceObjectGetInstanceId)(GDExtensionConstObjectPtr p_object);

/**
 * @name object_has_script_method
 * @since 4.3
 *
 * Checks if this object has a script with the given method.
 *
 * @param p_object A pointer to the Object.
 * @param p_method A pointer to a StringName identifying the method.
 *
 * @return true if the object has a script and that script has a method with the given name. Returns false if the object has no script.
 */
typedef GDExtensionBool (*GDExtensionInterfaceObjectHasScriptMethod)(GDExtensionConstObjectPtr p_object, GDExtensionConstStringNamePtr p_method);

/**
 * @name object_call_script_method
 * @since 4.3
 *
 * Call the given script method on this object.
 *
 * @param p_object A pointer to the Object.
 * @param p_method A pointer to a StringName identifying the method.
 * @param p_args A pointer to a C array of Variant.
 * @param p_argument_count The number of arguments.
 * @param r_return A pointer a Variant which will be assigned the return value.
 * @param r_error A pointer the structure which will hold error information.
 */
typedef void (*GDExtensionInterfaceObjectCallScriptMethod)(GDExtensionObjectPtr p_object, GDExtensionConstStringNamePtr p_method, const GDExtensionConstVariantPtr *p_args, GDExtensionInt p_argument_count, GDExtensionUninitializedVariantPtr r_return, GDExtensionCallError *r_error);

/**
 * @name ref_get_object
 * @since 4.1
 *
 * Gets the Object from a reference.
 *
 * @param p_ref A pointer to the reference.
 *
 * @return A pointer to the Object from the reference or NULL.
 */
typedef GDExtensionObjectPtr (*GDExtensionInterfaceRefGetObject)(GDExtensionConstRefPtr p_ref);

/**
 * @name ref_set_object
 * @since 4.1
 *
 * Sets the Object referred to by a reference.
 *
 * @param p_ref A pointer to the reference.
 * @param p_object A pointer to the Object to refer to.
 */
typedef void (*GDExtensionInterfaceRefSetObject)(GDExtensionRefPtr p_ref, GDExtensionObjectPtr p_object);

/**
 * @name script_instance_create
 * @since 4.1
 * @deprecated Deprecated in Godot 4.2. Use `script_instance_create3` instead.
 *
 * Creates a script instance that contains the given info and instance data.
 *
 * @param p_info A pointer to a GDExtensionScriptInstanceInfo struct.
 * @param p_instance_data A pointer to a data representing the script instance in the GDExtension. This will be passed to all the function pointers on p_info.
 *
 * @return A pointer to a ScriptInstanceExtension object.
 */
typedef GDExtensionScriptInstancePtr (*GDExtensionInterfaceScriptInstanceCreate)(const GDExtensionScriptInstanceInfo *p_info, GDExtensionScriptInstanceDataPtr p_instance_data);

/**
 * @name script_instance_create2
 * @since 4.2
 * @deprecated Deprecated in Godot 4.3. Use `script_instance_create3` instead.
 *
 * Creates a script instance that contains the given info and instance data.
 *
 * @param p_info A pointer to a GDExtensionScriptInstanceInfo2 struct.
 * @param p_instance_data A pointer to a data representing the script instance in the GDExtension. This will be passed to all the function pointers on p_info.
 *
 * @return A pointer to a ScriptInstanceExtension object.
 */
typedef GDExtensionScriptInstancePtr (*GDExtensionInterfaceScriptInstanceCreate2)(const GDExtensionScriptInstanceInfo2 *p_info, GDExtensionScriptInstanceDataPtr p_instance_data);

/**
 * @name script_instance_create3
 * @since 4.3
 *
 * Creates a script instance that contains the given info and instance data.
 *
 * @param p_info A pointer to a GDExtensionScriptInstanceInfo3 struct.
 * @param p_instance_data A pointer to a data representing the script instance in the GDExtension. This will be passed to all the function pointers on p_info.
 *
 * @return A pointer to a ScriptInstanceExtension object.
 */
typedef GDExtensionScriptInstancePtr (*GDExtensionInterfaceScriptInstanceCreate3)(const GDExtensionScriptInstanceInfo3 *p_info, GDExtensionScriptInstanceDataPtr p_instance_data);

/**
 * @name placeholder_script_instance_create
 * @since 4.2
 *
 * Creates a placeholder script instance for a given script and instance.
 *
 * This interface is optional as a custom placeholder could also be created with script_instance_create().
 *
 * @param p_language A pointer to a ScriptLanguage.
 * @param p_script A pointer to a Script.
 * @param p_owner A pointer to an Object.
 *
 * @return A pointer to a PlaceHolderScriptInstance object.
 */
typedef GDExtensionScriptInstancePtr (*GDExtensionInterfacePlaceholderScriptInstanceCreate)(GDExtensionObjectPtr p_language, GDExtensionObjectPtr p_script, GDExtensionObjectPtr p_owner);

/**
 * @name placeholder_script_instance_update
 * @since 4.2
 *
 * Updates a placeholder script instance with the given properties and values.
 *
 * The passed in placeholder must be an instance of PlaceHolderScriptInstance
 * such as the one returned by placeholder_script_instance_create().
 *
 * @param p_placeholder A pointer to a PlaceHolderScriptInstance.
 * @param p_properties A pointer to an Array of Dictionary representing PropertyInfo.
 * @param p_values A pointer to a Dictionary mapping StringName to Variant values.
 */
typedef void (*GDExtensionInterfacePlaceholderScriptInstanceUpdate)(GDExtensionScriptInstancePtr p_placeholder, GDExtensionConstTypePtr p_properties, GDExtensionConstTypePtr p_values);

/**
 * @name object_get_script_instance
 * @since 4.2
 *
 * Get the script instance data attached to this object.
 *
 * @param p_object A pointer to the Object.
 * @param p_language A pointer to the language expected for this script instance.
 *
 * @return A GDExtensionScriptInstanceDataPtr that was attached to this object as part of script_instance_create.
 */
typedef GDExtensionScriptInstanceDataPtr (*GDExtensionInterfaceObjectGetScriptInstance)(GDExtensionConstObjectPtr p_object, GDExtensionObjectPtr p_language);

/**
 * @name object_set_script_instance
 * @since 4.5
 *
 * Set the script instance data attached to this object.
 *
 * @param p_object A pointer to the Object.
 * @param p_script_instance A pointer to the script instance data to attach to this object.
 */
typedef void (*GDExtensionInterfaceObjectSetScriptInstance)(GDExtensionObjectPtr p_object, GDExtensionScriptInstanceDataPtr p_script_instance);

/**
 * @name callable_custom_create
 * @since 4.2
 * @deprecated Deprecated in Godot 4.3. Use `callable_custom_create2` instead.
 *
 * Creates a custom Callable object from a function pointer.
 *
 * Provided struct can be safely freed once the function returns.
 *
 * @param r_callable A pointer that will receive the new Callable.
 * @param p_callable_custom_info The info required to construct a Callable.
 */
typedef void (*GDExtensionInterfaceCallableCustomCreate)(GDExtensionUninitializedTypePtr r_callable, GDExtensionCallableCustomInfo *p_callable_custom_info);

/**
 * @name callable_custom_create2
 * @since 4.3
 *
 * Creates a custom Callable object from a function pointer.
 *
 * Provided struct can be safely freed once the function returns.
 *
 * @param r_callable A pointer that will receive the new Callable.
 * @param p_callable_custom_info The info required to construct a Callable.
 */
typedef void (*GDExtensionInterfaceCallableCustomCreate2)(GDExtensionUninitializedTypePtr r_callable, GDExtensionCallableCustomInfo2 *p_callable_custom_info);

/**
 * @name callable_custom_get_userdata
 * @since 4.2
 *
 * Retrieves the userdata pointer from a custom Callable.
 *
 * If the Callable is not a custom Callable or the token does not match the one provided to callable_custom_create() via GDExtensionCallableCustomInfo then NULL will be returned.
 *
 * @param p_callable A pointer to a Callable.
 * @param p_token A pointer to an address that uniquely identifies the GDExtension.
 *
 * @return The userdata pointer given when creating this custom Callable.
 */
typedef void *(*GDExtensionInterfaceCallableCustomGetUserdata)(GDExtensionConstTypePtr p_callable, void *p_token);

/**
 * @name classdb_construct_object
 * @since 4.1
 * @deprecated Deprecated in Godot 4.4. Use `classdb_construct_object3` instead.
 *
 * Constructs an Object of the requested class.
 *
 * The passed class must be a built-in godot class, or an already-registered extension class. In both cases, object_set_instance() should be called to fully initialize the object.
 *
 * @param p_classname A pointer to a StringName with the class name.
 *
 * @return A pointer to the newly created Object.
 */
typedef GDExtensionObjectPtr (*GDExtensionInterfaceClassdbConstructObject)(GDExtensionConstStringNamePtr p_classname);

/**
 * @name classdb_construct_object2
 * @since 4.4
 * @deprecated Deprecated in Godot 4.7. Use `classdb_construct_object3` instead.
 *
 * Constructs an Object of the requested class.
 *
 * The passed class must be a built-in godot class, or an already-registered extension class. In both cases, object_set_instance() should be called to fully initialize the object.
 *
 * "NOTIFICATION_POSTINITIALIZE" must be sent after construction.
 *
 * @param p_classname A pointer to a StringName with the class name.
 *
 * @return A pointer to the newly created Object.
 */
typedef GDExtensionObjectPtr (*GDExtensionInterfaceClassdbConstructObject2)(GDExtensionConstStringNamePtr p_classname);

/**
 * @name classdb_construct_object3
 * @since 4.7
 *
 * Constructs an Object of the requested class.
 *
 * The passed class must be a built-in godot class, or an already-registered extension class. In both cases, object_set_instance() should be called to fully initialize the object.
 * If the type is a subtype of RefCounted, it already has a refcount of 1. The caller must take ownership the refcount and is responsible for decrementing it again when the object is no longer needed.
 *
 * "NOTIFICATION_POSTINITIALIZE" must be sent after construction.
 *
 * @param p_classname A pointer to a StringName with the class name.
 *
 * @return A pointer to the newly created Object.
 */
typedef GDExtensionObjectPtr (*GDExtensionInterfaceClassdbConstructObject3)(GDExtensionConstStringNamePtr p_classname);

/**
 * @name classdb_get_method_bind
 * @since 4.1
 *
 * Gets a pointer to the MethodBind in ClassDB for the given class, method and hash.
 *
 * @param p_classname A pointer to a StringName with the class name.
 * @param p_methodname A pointer to a StringName with the method name.
 * @param p_hash A hash representing the function signature.
 *
 * @return A pointer to the MethodBind from ClassDB.
 */
typedef GDExtensionMethodBindPtr (*GDExtensionInterfaceClassdbGetMethodBind)(GDExtensionConstStringNamePtr p_classname, GDExtensionConstStringNamePtr p_methodname, GDExtensionInt p_hash);

/**
 * @name classdb_get_class_tag
 * @since 4.1
 * @deprecated Deprecated in Godot 4.7. No longer needed. Use the `is_class` method on `Object` instead.
 *
 * Gets a pointer uniquely identifying the given built-in class in the ClassDB.
 *
 * @param p_classname A pointer to a StringName with the class name.
 *
 * @return A pointer uniquely identifying the built-in class in the ClassDB.
 */
typedef void *(*GDExtensionInterfaceClassdbGetClassTag)(GDExtensionConstStringNamePtr p_classname);

/**
 * @name classdb_register_extension_class
 * @since 4.1
 * @deprecated Deprecated in Godot 4.2. Use `classdb_register_extension_class6` instead.
 *
 * Registers an extension class in the ClassDB.
 *
 * Provided struct can be safely freed once the function returns.
 *
 * @param p_library A pointer the library received by the GDExtension's entry point function.
 * @param p_class_name A pointer to a StringName with the class name.
 * @param p_parent_class_name A pointer to a StringName with the parent class name.
 * @param p_extension_funcs A pointer to a GDExtensionClassCreationInfo struct.
 */
typedef void (*GDExtensionInterfaceClassdbRegisterExtensionClass)(GDExtensionClassLibraryPtr p_library, GDExtensionConstStringNamePtr p_class_name, GDExtensionConstStringNamePtr p_parent_class_name, const GDExtensionClassCreationInfo *p_extension_funcs);

/**
 * @name classdb_register_extension_class2
 * @since 4.2
 * @deprecated Deprecated in Godot 4.3. Use `classdb_register_extension_class6` instead.
 *
 * Registers an extension class in the ClassDB.
 *
 * Provided struct can be safely freed once the function returns.
 *
 * @param p_library A pointer the library received by the GDExtension's entry point function.
 * @param p_class_name A pointer to a StringName with the class name.
 * @param p_parent_class_name A pointer to a StringName with the parent class name.
 * @param p_extension_funcs A pointer to a GDExtensionClassCreationInfo2 struct.
 */
typedef void (*GDExtensionInterfaceClassdbRegisterExtensionClass2)(GDExtensionClassLibraryPtr p_library, GDExtensionConstStringNamePtr p_class_name, GDExtensionConstStringNamePtr p_parent_class_name, const GDExtensionClassCreationInfo2 *p_extension_funcs);

/**
 * @name classdb_register_extension_class3
 * @since 4.3
 * @deprecated Deprecated in Godot 4.4. Use `classdb_register_extension_class6` instead.
 *
 * Registers an extension class in the ClassDB.
 *
 * Provided struct can be safely freed once the function returns.
 *
 * @param p_library A pointer the library received by the GDExtension's entry point function.
 * @param p_class_name A pointer to a StringName with the class name.
 * @param p_parent_class_name A pointer to a StringName with the parent class name.
 * @param p_extension_funcs A pointer to a GDExtensionClassCreationInfo3 struct.
 */
typedef void (*GDExtensionInterfaceClassdbRegisterExtensionClass3)(GDExtensionClassLibraryPtr p_library, GDExtensionConstStringNamePtr p_class_name, GDExtensionConstStringNamePtr p_parent_class_name, const GDExtensionClassCreationInfo3 *p_extension_funcs);

/**
 * @name classdb_register_extension_class4
 * @since 4.4
 * @deprecated Deprecated in Godot 4.5. Use `classdb_register_extension_class6` instead.
 *
 * Registers an extension class in the ClassDB.
 *
 * Provided struct can be safely freed once the function returns.
 *
 * @param p_library A pointer the library received by the GDExtension's entry point function.
 * @param p_class_name A pointer to a StringName with the class name.
 * @param p_parent_class_name A pointer to a StringName with the parent class name.
 * @param p_extension_funcs A pointer to a GDExtensionClassCreationInfo4 struct.
 */
typedef void (*GDExtensionInterfaceClassdbRegisterExtensionClass4)(GDExtensionClassLibraryPtr p_library, GDExtensionConstStringNamePtr p_class_name, GDExtensionConstStringNamePtr p_parent_class_name, const GDExtensionClassCreationInfo4 *p_extension_funcs);

/**
 * @name classdb_register_extension_class5
 * @since 4.5
 * @deprecated Deprecated in Godot 4.7. Use `classdb_register_extension_class6` instead.
 *
 * Registers an extension class in the ClassDB.
 *
 * Provided struct can be safely freed once the function returns.
 *
 * @param p_library A pointer the library received by the GDExtension's entry point function.
 * @param p_class_name A pointer to a StringName with the class name.
 * @param p_parent_class_name A pointer to a StringName with the parent class name.
 * @param p_extension_funcs A pointer to a GDExtensionClassCreationInfo5 struct.
 */
typedef void (*GDExtensionInterfaceClassdbRegisterExtensionClass5)(GDExtensionClassLibraryPtr p_library, GDExtensionConstStringNamePtr p_class_name, GDExtensionConstStringNamePtr p_parent_class_name, const GDExtensionClassCreationInfo5 *p_extension_funcs);

/**
 * @name classdb_register_extension_class6
 * @since 4.7
 *
 * Registers an extension class in the ClassDB.
 *
 * Provided struct can be safely freed once the function returns.
 *
 * @param p_library A pointer the library received by the GDExtension's entry point function.
 * @param p_class_name A pointer to a StringName with the class name.
 * @param p_parent_class_name A pointer to a StringName with the parent class name.
 * @param p_extension_funcs A pointer to a GDExtensionClassCreationInfo6 struct.
 */
typedef void (*GDExtensionInterfaceClassdbRegisterExtensionClass6)(GDExtensionClassLibraryPtr p_library, GDExtensionConstStringNamePtr p_class_name, GDExtensionConstStringNamePtr p_parent_class_name, const GDExtensionClassCreationInfo6 *p_extension_funcs);

/**
 * @name classdb_register_extension_class_method
 * @since 4.1
 *
 * Registers a method on an extension class in the ClassDB.
 *
 * Provided struct can be safely freed once the function returns.
 *
 * @param p_library A pointer the library received by the GDExtension's entry point function.
 * @param p_class_name A pointer to a StringName with the class name.
 * @param p_method_info A pointer to a GDExtensionClassMethodInfo struct.
 */
typedef void (*GDExtensionInterfaceClassdbRegisterExtensionClassMethod)(GDExtensionClassLibraryPtr p_library, GDExtensionConstStringNamePtr p_class_name, const GDExtensionClassMethodInfo *p_method_info);

/**
 * @name classdb_register_extension_class_virtual_method
 * @since 4.3
 *
 * Registers a virtual method on an extension class in ClassDB, that can be implemented by scripts or other extensions.
 *
 * Provided struct can be safely freed once the function returns.
 *
 * @param p_library A pointer the library received by the GDExtension's entry point function.
 * @param p_class_name A pointer to a StringName with the class name.
 * @param p_method_info A pointer to a GDExtensionClassMethodInfo struct.
 */
typedef void (*GDExtensionInterfaceClassdbRegisterExtensionClassVirtualMethod)(GDExtensionClassLibraryPtr p_library, GDExtensionConstStringNamePtr p_class_name, const GDExtensionClassVirtualMethodInfo *p_method_info);

/**
 * @name classdb_register_extension_class_integer_constant
 * @since 4.1
 *
 * Registers an integer constant on an extension class in the ClassDB.
 *
 * Note about registering bitfield values (if p_is_bitfield is true): even though p_constant_value is signed, language bindings are
 * advised to treat bitfields as uint64_t, since this is generally clearer and can prevent mistakes like using -1 for setting all bits.
 * Language APIs should thus provide an abstraction that registers bitfields (uint64_t) separately from regular constants (int64_t).
 *
 * @param p_library A pointer the library received by the GDExtension's entry point function.
 * @param p_class_name A pointer to a StringName with the class name.
 * @param p_enum_name A pointer to a StringName with the enum name.
 * @param p_constant_name A pointer to a StringName with the constant name.
 * @param p_constant_value The constant value.
 * @param p_is_bitfield Whether or not this constant is part of a bitfield.
 */
typedef void (*GDExtensionInterfaceClassdbRegisterExtensionClassIntegerConstant)(GDExtensionClassLibraryPtr p_library, GDExtensionConstStringNamePtr p_class_name, GDExtensionConstStringNamePtr p_enum_name, GDExtensionConstStringNamePtr p_constant_name, GDExtensionInt p_constant_value, GDExtensionBool p_is_bitfield);

/**
 * @name classdb_register_extension_class_property
 * @since 4.1
 *
 * Registers a property on an extension class in the ClassDB.
 *
 * Provided struct can be safely freed once the function returns.
 *
 * @param p_library A pointer the library received by the GDExtension's entry point function.
 * @param p_class_name A pointer to a StringName with the class name.
 * @param p_info A pointer to a GDExtensionPropertyInfo struct.
 * @param p_setter A pointer to a StringName with the name of the setter method.
 * @param p_getter A pointer to a StringName with the name of the getter method.
 */
typedef void (*GDExtensionInterfaceClassdbRegisterExtensionClassProperty)(GDExtensionClassLibraryPtr p_library, GDExtensionConstStringNamePtr p_class_name, const GDExtensionPropertyInfo *p_info, GDExtensionConstStringNamePtr p_setter, GDExtensionConstStringNamePtr p_getter);

/**
 * @name classdb_register_extension_class_property_indexed
 * @since 4.2
 *
 * Registers an indexed property on an extension class in the ClassDB.
 *
 * Provided struct can be safely freed once the function returns.
 *
 * @param p_library A pointer the library received by the GDExtension's entry point function.
 * @param p_class_name A pointer to a StringName with the class name.
 * @param p_info A pointer to a GDExtensionPropertyInfo struct.
 * @param p_setter A pointer to a StringName with the name of the setter method.
 * @param p_getter A pointer to a StringName with the name of the getter method.
 * @param p_index The index to pass as the first argument to the getter and setter methods.
 */
typedef void (*GDExtensionInterfaceClassdbRegisterExtensionClassPropertyIndexed)(GDExtensionClassLibraryPtr p_library, GDExtensionConstStringNamePtr p_class_name, const GDExtensionPropertyInfo *p_info, GDExtensionConstStringNamePtr p_setter, GDExtensionConstStringNamePtr p_getter, GDExtensionInt p_index);

/**
 * @name classdb_register_extension_class_property_group
 * @since 4.1
 *
 * Registers a property group on an extension class in the ClassDB.
 *
 * @param p_library A pointer the library received by the GDExtension's entry point function.
 * @param p_class_name A pointer to a StringName with the class name.
 * @param p_group_name A pointer to a String with the group name.
 * @param p_prefix A pointer to a String with the prefix used by properties in this group.
 */
typedef void (*GDExtensionInterfaceClassdbRegisterExtensionClassPropertyGroup)(GDExtensionClassLibraryPtr p_library, GDExtensionConstStringNamePtr p_class_name, GDExtensionConstStringPtr p_group_name, GDExtensionConstStringPtr p_prefix);

/**
 * @name classdb_register_extension_class_property_subgroup
 * @since 4.1
 *
 * Registers a property subgroup on an extension class in the ClassDB.
 *
 * @param p_library A pointer the library received by the GDExtension's entry point function.
 * @param p_class_name A pointer to a StringName with the class name.
 * @param p_subgroup_name A pointer to a String with the subgroup name.
 * @param p_prefix A pointer to a String with the prefix used by properties in this subgroup.
 */
typedef void (*GDExtensionInterfaceClassdbRegisterExtensionClassPropertySubgroup)(GDExtensionClassLibraryPtr p_library, GDExtensionConstStringNamePtr p_class_name, GDExtensionConstStringPtr p_subgroup_name, GDExtensionConstStringPtr p_prefix);

/**
 * @name classdb_register_extension_class_signal
 * @since 4.1
 *
 * Registers a signal on an extension class in the ClassDB.
 *
 * Provided structs can be safely freed once the function returns.
 *
 * @param p_library A pointer the library received by the GDExtension's entry point function.
 * @param p_class_name A pointer to a StringName with the class name.
 * @param p_signal_name A pointer to a StringName with the signal name.
 * @param p_argument_info A pointer to a GDExtensionPropertyInfo struct.
 * @param p_argument_count The number of arguments the signal receives.
 */
typedef void (*GDExtensionInterfaceClassdbRegisterExtensionClassSignal)(GDExtensionClassLibraryPtr p_library, GDExtensionConstStringNamePtr p_class_name, GDExtensionConstStringNamePtr p_signal_name, const GDExtensionPropertyInfo *p_argument_info, GDExtensionInt p_argument_count);

/**
 * @name classdb_unregister_extension_class
 * @since 4.1
 *
 * Unregisters an extension class in the ClassDB.
 *
 * Unregistering a parent class before a class that inherits it will result in failure. Inheritors must be unregistered first.
 *
 * @param p_library A pointer the library received by the GDExtension's entry point function.
 * @param p_class_name A pointer to a StringName with the class name.
 */
typedef void (*GDExtensionInterfaceClassdbUnregisterExtensionClass)(GDExtensionClassLibraryPtr p_library, GDExtensionConstStringNamePtr p_class_name);

/**
 * @name get_library_path
 * @since 4.1
 *
 * Gets the path to the current GDExtension library.
 *
 * @param p_library A pointer the library received by the GDExtension's entry point function.
 * @param r_path A pointer to a String which will receive the path.
 */
typedef void (*GDExtensionInterfaceGetLibraryPath)(GDExtensionClassLibraryPtr p_library, GDExtensionUninitializedStringPtr r_path);

/**
 * @name editor_add_plugin
 * @since 4.1
 *
 * Adds an editor plugin.
 *
 * It's safe to call during initialization.
 *
 * @param p_class_name A pointer to a StringName with the name of a class (descending from EditorPlugin) which is already registered with ClassDB.
 */
typedef void (*GDExtensionInterfaceEditorAddPlugin)(GDExtensionConstStringNamePtr p_class_name);

/**
 * @name editor_remove_plugin
 * @since 4.1
 *
 * Removes an editor plugin.
 *
 * @param p_class_name A pointer to a StringName with the name of a class that was previously added as an editor plugin.
 */
typedef void (*GDExtensionInterfaceEditorRemovePlugin)(GDExtensionConstStringNamePtr p_class_name);

/**
 * @name editor_help_load_xml_from_utf8_chars
 * @since 4.3
 *
 * Loads new XML-formatted documentation data in the editor.
 *
 * The provided pointer can be immediately freed once the function returns.
 *
 * @param p_data A pointer to a UTF-8 encoded C string (null terminated).
 */
typedef void (*GDExtensionInterfaceEditorHelpLoadXmlFromUtf8Chars)(const char *p_data);

/**
 * @name editor_help_load_xml_from_utf8_chars_and_len
 * @since 4.3
 *
 * Loads new XML-formatted documentation data in the editor.
 *
 * The provided pointer can be immediately freed once the function returns.
 *
 * @param p_data A pointer to a UTF-8 encoded C string.
 * @param p_size The number of bytes (not code units).
 */
typedef void (*GDExtensionInterfaceEditorHelpLoadXmlFromUtf8CharsAndLen)(const char *p_data, GDExtensionInt p_size);

/**
 * @name editor_register_get_classes_used_callback
 * @since 4.5
 *
 * Registers a callback that Godot can call to get the list of all classes (from ClassDB) that may be used by the calling GDExtension.
 *
 * This is used by the editor to generate a build profile (in "Tools" > "Engine Compilation Configuration Editor..." > "Detect from project"),
 * in order to recompile Godot with only the classes used.
 * In the provided callback, the GDExtension should provide the list of classes that _may_ be used statically, thus the time of invocation shouldn't matter.
 * If a GDExtension doesn't register a callback, Godot will assume that it could be using any classes.
 *
 * @param p_library A pointer the library received by the GDExtension's entry point function.
 * @param p_callback The callback to retrieve the list of classes used.
 */
typedef void (*GDExtensionInterfaceEditorRegisterGetClassesUsedCallback)(GDExtensionClassLibraryPtr p_library, GDExtensionEditorGetClassesUsedCallback p_callback);

/**
 * @name register_main_loop_callbacks
 * @since 4.5
 *
 * Registers callbacks to be called at different phases of the main loop.
 *
 * @param p_library A pointer the library received by the GDExtension's entry point function.
 * @param p_callbacks A pointer to the structure that contains the callbacks.
 */
typedef void (*GDExtensionInterfaceRegisterMainLoopCallbacks)(GDExtensionClassLibraryPtr p_library, const GDExtensionMainLoopCallbacks *p_callbacks);

/**
 * @name device_exit_bootstrap_library_v1
 * @since 4.7
 *
 * Borrow the current loader bootstrap authority. The library must match the exact current nested loader scope; the capability expires when that scope returns.
 *
 * Metadata only: this reference never authorizes DSO entry, device work or unload. Handles are process-local; each retained or cloned handle may be released once. Callers must supply valid aligned native pointers and must not alias input and output storage.
 * Request headers use ABI version 1, struct_size 16 and reserved zero. Writable capability outputs are zeroed on errors; null or misaligned outputs are not written. Threaded builds allow view transfer between threads; thread-disabled builds require single-threaded use.
 *
 * @param request Version 1 request header.
 * @param library Exact current bootstrap library pointer or borrowed capability.
 * @param out_library Receives borrowed bootstrap authority.
 *
 * @return Existing Godot Error numeric value.
 */
typedef int32_t (*GDExtensionInterfaceDeviceExitBootstrapLibraryV1)(const GDExtensionDeviceExitAbi *request, GDExtensionClassLibraryPtr library, GDExtensionDeviceExitLibraryCapability *out_library);

/**
 * @name device_exit_retain_library_view_v1
 * @since 4.7
 *
 * Create a separately releasable view during the matching bootstrap scope.
 *
 * Metadata only: this reference never authorizes DSO entry, device work or unload. Handles are process-local; each retained or cloned handle may be released once. Callers must supply valid aligned native pointers and must not alias input and output storage.
 * Request headers use ABI version 1, struct_size 16 and reserved zero. Writable capability outputs are zeroed on errors; null or misaligned outputs are not written. Threaded builds allow view transfer between threads; thread-disabled builds require single-threaded use.
 *
 * @param request Version 1 request header.
 * @param library Exact current bootstrap library pointer or borrowed capability.
 * @param out_view Receives a fresh view reference.
 *
 * @return Existing Godot Error numeric value.
 */
typedef int32_t (*GDExtensionInterfaceDeviceExitRetainLibraryViewV1)(const GDExtensionDeviceExitAbi *request, GDExtensionDeviceExitLibraryCapability library, GDExtensionDeviceExitLibraryViewRef *out_view);

/**
 * @name device_exit_copy_library_snapshot_v1
 * @since 4.7
 *
 * Copy one consistent 48-byte snapshot. The caller supplies ABI version 1, struct_size 48, and zero reserved header; errors leave the entire output unchanged. Detached describes only the wrapper association, not OS unload. Initialize error is valid only if loader_initialize_returned is 1. guarded_qualified is zero in this foundation.
 *
 * Metadata only: this reference never authorizes DSO entry, device work or unload. Handles are process-local; each retained or cloned handle may be released once. Callers must supply valid aligned native pointers and must not alias input and output storage.
 * Request headers use ABI version 1, struct_size 16 and reserved zero. Writable capability outputs are zeroed on errors; null or misaligned outputs are not written. Threaded builds allow view transfer between threads; thread-disabled builds require single-threaded use.
 *
 * @param view A live metadata view.
 * @param out_snapshot Caller-owned versioned snapshot buffer.
 *
 * @return Existing Godot Error numeric value.
 */
typedef int32_t (*GDExtensionInterfaceDeviceExitCopyLibrarySnapshotV1)(GDExtensionDeviceExitLibraryViewRef view, GDExtensionDeviceExitLibrarySnapshot *out_snapshot);

/**
 * @name device_exit_release_library_view_v1
 * @since 4.7
 *
 * Consume one view reference. Duplicate or stale release returns ERR_INVALID_DATA without affecting other references. Copy and release remain available while the registry finalizes.
 *
 * Metadata only: this reference never authorizes DSO entry, device work or unload. Handles are process-local; each retained or cloned handle may be released once. Callers must supply valid aligned native pointers and must not alias input and output storage.
 * Request headers use ABI version 1, struct_size 16 and reserved zero. Writable capability outputs are zeroed on errors; null or misaligned outputs are not written. Threaded builds allow view transfer between threads; thread-disabled builds require single-threaded use.
 *
 * @param request Version 1 request header.
 * @param view A live metadata view.
 *
 * @return Existing Godot Error numeric value.
 */
typedef int32_t (*GDExtensionInterfaceDeviceExitReleaseLibraryViewV1)(const GDExtensionDeviceExitAbi *request, GDExtensionDeviceExitLibraryViewRef view);

/**
 * @name device_exit_clone_library_view_v1
 * @since 4.7
 *
 * Create a fresh one-use reference to the same metadata from an existing view. Running allows clones, including detached records; Finalizing refuses new clones with ERR_BUSY.
 *
 * Metadata only: this reference never authorizes DSO entry, device work or unload. Handles are process-local; each retained or cloned handle may be released once. Callers must supply valid aligned native pointers and must not alias input and output storage.
 * Request headers use ABI version 1, struct_size 16 and reserved zero. Writable capability outputs are zeroed on errors; null or misaligned outputs are not written. Threaded builds allow view transfer between threads; thread-disabled builds require single-threaded use.
 *
 * @param request Version 1 request header.
 * @param source The live view to clone.
 * @param out_view Receives a fresh view reference.
 *
 * @return Existing Godot Error numeric value.
 */
typedef int32_t (*GDExtensionInterfaceDeviceExitCloneLibraryViewV1)(const GDExtensionDeviceExitAbi *request, GDExtensionDeviceExitLibraryViewRef source, GDExtensionDeviceExitLibraryViewRef *out_view);

/**
 * @name device_exit_register_guard_v1
 * @since 4.7
 *
 * Internal reciprocal guarded-lifetime v1 surface. Availability does not enable guarded loading; the complete loader, binding and entry census gate remains required.
 *
 * Native aligned, nonaliasing pointers are required. Abi uses version 1, sizeof(GDExtensionDeviceExitAbi), reserved zero; embedded registration and diagnostic headers use their own generated sizeof.
 * Borrowed contexts and cleanup handles expire at the exact issuing callback return. The guard token lasts until enrollment revocation. Code holds persist across frames and retire after release at the original outer callback return. Empty outputs remain empty on failure; nonempty outputs are rejected unchanged.
 *
 * @param library Library.
 * @param registration Registration.
 * @param out_token Out token.
 *
 * @return Existing Godot Error value.
 */
typedef int32_t (*GDExtensionInterfaceDeviceExitRegisterGuardV1)(GDExtensionClassLibraryPtr library, const GDExtensionDeviceExitGuardRegistrationV1 *registration, GDExtensionDeviceExitGuardToken *out_token);

/**
 * @name device_exit_borrow_guarded_library_v1
 * @since 4.7
 *
 * Internal reciprocal guarded-lifetime v1 surface. Availability does not enable guarded loading; the complete loader, binding and entry census gate remains required.
 *
 * Native aligned, nonaliasing pointers are required. Abi uses version 1, sizeof(GDExtensionDeviceExitAbi), reserved zero; embedded registration and diagnostic headers use their own generated sizeof.
 * Borrowed contexts and cleanup handles expire at the exact issuing callback return. The guard token lasts until enrollment revocation. Code holds persist across frames and retire after release at the original outer callback return. Empty outputs remain empty on failure; nonempty outputs are rejected unchanged.
 *
 * @param abi Abi.
 * @param library Library.
 * @param out_library Out library.
 *
 * @return Existing Godot Error value.
 */
typedef int32_t (*GDExtensionInterfaceDeviceExitBorrowGuardedLibraryV1)(const GDExtensionDeviceExitAbi *abi, GDExtensionClassLibraryPtr library, GDExtensionDeviceExitGuardedLibraryRef *out_library);

/**
 * @name device_exit_borrow_initialize_level_v1
 * @since 4.7
 *
 * Internal reciprocal guarded-lifetime v1 surface. Availability does not enable guarded loading; the complete loader, binding and entry census gate remains required.
 *
 * Native aligned, nonaliasing pointers are required. Abi uses version 1, sizeof(GDExtensionDeviceExitAbi), reserved zero; embedded registration and diagnostic headers use their own generated sizeof.
 * Borrowed contexts and cleanup handles expire at the exact issuing callback return. The guard token lasts until enrollment revocation. Code holds persist across frames and retire after release at the original outer callback return. Empty outputs remain empty on failure; nonempty outputs are rejected unchanged.
 *
 * @param abi Abi.
 * @param library Library.
 * @param level Level.
 * @param out_context Out context.
 *
 * @return Existing Godot Error value.
 */
typedef int32_t (*GDExtensionInterfaceDeviceExitBorrowInitializeLevelV1)(const GDExtensionDeviceExitAbi *abi, GDExtensionClassLibraryPtr library, uint32_t level, GDExtensionDeviceExitInitializationLevelContextRef *out_context);

/**
 * @name device_exit_borrow_deinit_level_v1
 * @since 4.7
 *
 * Internal reciprocal guarded-lifetime v1 surface. Availability does not enable guarded loading; the complete loader, binding and entry census gate remains required.
 *
 * Native aligned, nonaliasing pointers are required. Abi uses version 1, sizeof(GDExtensionDeviceExitAbi), reserved zero; embedded registration and diagnostic headers use their own generated sizeof.
 * Borrowed contexts and cleanup handles expire at the exact issuing callback return. The guard token lasts until enrollment revocation. Code holds persist across frames and retire after release at the original outer callback return. Empty outputs remain empty on failure; nonempty outputs are rejected unchanged.
 *
 * @param abi Abi.
 * @param library Library.
 * @param level Level.
 * @param out_context Out context.
 *
 * @return Existing Godot Error value.
 */
typedef int32_t (*GDExtensionInterfaceDeviceExitBorrowDeinitLevelV1)(const GDExtensionDeviceExitAbi *abi, GDExtensionClassLibraryPtr library, uint32_t level, GDExtensionDeviceExitDeinitLevelContextRef *out_context);

/**
 * @name device_exit_borrow_level_cleanup_v1
 * @since 4.7
 *
 * Internal reciprocal guarded-lifetime v1 surface. Availability does not enable guarded loading; the complete loader, binding and entry census gate remains required.
 *
 * Native aligned, nonaliasing pointers are required. Abi uses version 1, sizeof(GDExtensionDeviceExitAbi), reserved zero; embedded registration and diagnostic headers use their own generated sizeof.
 * Borrowed contexts and cleanup handles expire at the exact issuing callback return. The guard token lasts until enrollment revocation. Code holds persist across frames and retire after release at the original outer callback return. Empty outputs remain empty on failure; nonempty outputs are rejected unchanged.
 * Public phase is USER=0 or BINDING=1, not the internal cleanup enum.
 * Cleanup phase is USER=0 or BINDING=1; other values are rejected.
 *
 * @param abi Abi.
 * @param context Context.
 * @param phase Phase.
 * @param out_cleanup Out cleanup.
 *
 * @return Existing Godot Error value.
 */
typedef int32_t (*GDExtensionInterfaceDeviceExitBorrowLevelCleanupV1)(const GDExtensionDeviceExitAbi *abi, GDExtensionDeviceExitInitializationLevelContextRef context, uint32_t phase, GDExtensionDeviceExitRegisteredCleanupRef *out_cleanup);

/**
 * @name device_exit_schedule_object_cleanup_v1
 * @since 4.7
 *
 * Register an exact core Object binding cleanup as due in the borrowed USER or BINDING level action. The original Object action and all bound holders must exist in the same library generation; the original owner still performs the actual free.
 *
 * Object ID is the original core ObjectDB ID, not a pointer or caller-invented cleanup authority. Only the issuing initialization frame may enroll, before its binding acknowledgement.
 *
 * @param abi Abi.
 * @param context Original initialization level context.
 * @param terminal Borrowed USER or BINDING action for this exact level.
 * @param object_id Original core Object ID.
 *
 * @return Existing Godot Error value.
 */
typedef int32_t (*GDExtensionInterfaceDeviceExitScheduleObjectCleanupV1)(const GDExtensionDeviceExitAbi *abi, GDExtensionDeviceExitInitializationLevelContextRef context, GDExtensionDeviceExitRegisteredCleanupRef terminal, uint64_t object_id);

/**
 * @name device_exit_acquire_code_hold_v1
 * @since 4.7
 *
 * Internal reciprocal guarded-lifetime v1 surface. Availability does not enable guarded loading; the complete loader, binding and entry census gate remains required.
 *
 * Native aligned, nonaliasing pointers are required. Abi uses version 1, sizeof(GDExtensionDeviceExitAbi), reserved zero; embedded registration and diagnostic headers use their own generated sizeof.
 * Borrowed contexts and cleanup handles expire at the exact issuing callback return. The guard token lasts until enrollment revocation. Code holds persist across frames and retire after release at the original outer callback return. Empty outputs remain empty on failure; nonempty outputs are rejected unchanged.
 *
 * @param abi Abi.
 * @param library Library.
 * @param cleanup Cleanup.
 * @param out_hold Out hold.
 *
 * @return Existing Godot Error value.
 */
typedef int32_t (*GDExtensionInterfaceDeviceExitAcquireCodeHoldV1)(const GDExtensionDeviceExitAbi *abi, GDExtensionDeviceExitGuardedLibraryRef library, GDExtensionDeviceExitRegisteredCleanupRef cleanup, GDExtensionDeviceExitLibraryCodeHoldRef *out_hold);

/**
 * @name device_exit_clone_code_hold_v1
 * @since 4.7
 *
 * Internal reciprocal guarded-lifetime v1 surface. Availability does not enable guarded loading; the complete loader, binding and entry census gate remains required.
 *
 * Native aligned, nonaliasing pointers are required. Abi uses version 1, sizeof(GDExtensionDeviceExitAbi), reserved zero; embedded registration and diagnostic headers use their own generated sizeof.
 * Borrowed contexts and cleanup handles expire at the exact issuing callback return. The guard token lasts until enrollment revocation. Code holds persist across frames and retire after release at the original outer callback return. Empty outputs remain empty on failure; nonempty outputs are rejected unchanged.
 *
 * @param abi Abi.
 * @param source Source.
 * @param out_hold Out hold.
 *
 * @return Existing Godot Error value.
 */
typedef int32_t (*GDExtensionInterfaceDeviceExitCloneCodeHoldV1)(const GDExtensionDeviceExitAbi *abi, GDExtensionDeviceExitLibraryCodeHoldRef source, GDExtensionDeviceExitLibraryCodeHoldRef *out_hold);

/**
 * @name device_exit_release_code_hold_v1
 * @since 4.7
 *
 * Internal reciprocal guarded-lifetime v1 surface. Availability does not enable guarded loading; the complete loader, binding and entry census gate remains required.
 *
 * Native aligned, nonaliasing pointers are required. Abi uses version 1, sizeof(GDExtensionDeviceExitAbi), reserved zero; embedded registration and diagnostic headers use their own generated sizeof.
 * Borrowed contexts and cleanup handles expire at the exact issuing callback return. The guard token lasts until enrollment revocation. Code holds persist across frames and retire after release at the original outer callback return. Empty outputs remain empty on failure; nonempty outputs are rejected unchanged.
 *
 * @param abi Abi.
 * @param hold Hold.
 *
 * @return Existing Godot Error value.
 */
typedef int32_t (*GDExtensionInterfaceDeviceExitReleaseCodeHoldV1)(const GDExtensionDeviceExitAbi *abi, GDExtensionDeviceExitLibraryCodeHoldRef hold);

/**
 * @name device_exit_after_binding_initialize_v1
 * @since 4.7
 *
 * Internal reciprocal guarded-lifetime v1 surface. Availability does not enable guarded loading; the complete loader, binding and entry census gate remains required.
 *
 * Native aligned, nonaliasing pointers are required. Abi uses version 1, sizeof(GDExtensionDeviceExitAbi), reserved zero; embedded registration and diagnostic headers use their own generated sizeof.
 * Borrowed contexts and cleanup handles expire at the exact issuing callback return. The guard token lasts until enrollment revocation. Code holds persist across frames and retire after release at the original outer callback return. Empty outputs remain empty on failure; nonempty outputs are rejected unchanged.
 *
 * @param abi Abi.
 * @param context Context.
 *
 * @return Existing Godot Error value.
 */
typedef int32_t (*GDExtensionInterfaceDeviceExitAfterBindingInitializeV1)(const GDExtensionDeviceExitAbi *abi, GDExtensionDeviceExitInitializationLevelContextRef context);

/**
 * @name device_exit_begin_user_terminator_v1
 * @since 4.7
 *
 * Internal reciprocal guarded-lifetime v1 surface. Availability does not enable guarded loading; the complete loader, binding and entry census gate remains required.
 *
 * Native aligned, nonaliasing pointers are required. Abi uses version 1, sizeof(GDExtensionDeviceExitAbi), reserved zero; embedded registration and diagnostic headers use their own generated sizeof.
 * Borrowed contexts and cleanup handles expire at the exact issuing callback return. The guard token lasts until enrollment revocation. Code holds persist across frames and retire after release at the original outer callback return. Empty outputs remain empty on failure; nonempty outputs are rejected unchanged.
 *
 * @param abi Abi.
 * @param token Token.
 * @param context Context.
 *
 * @return Existing Godot Error value.
 */
typedef int32_t (*GDExtensionInterfaceDeviceExitBeginUserTerminatorV1)(const GDExtensionDeviceExitAbi *abi, GDExtensionDeviceExitGuardToken token, GDExtensionDeviceExitDeinitLevelContextRef context);

/**
 * @name device_exit_after_user_terminator_v1
 * @since 4.7
 *
 * Internal reciprocal guarded-lifetime v1 surface. Availability does not enable guarded loading; the complete loader, binding and entry census gate remains required.
 *
 * Native aligned, nonaliasing pointers are required. Abi uses version 1, sizeof(GDExtensionDeviceExitAbi), reserved zero; embedded registration and diagnostic headers use their own generated sizeof.
 * Borrowed contexts and cleanup handles expire at the exact issuing callback return. The guard token lasts until enrollment revocation. Code holds persist across frames and retire after release at the original outer callback return. Empty outputs remain empty on failure; nonempty outputs are rejected unchanged.
 *
 * @param abi Abi.
 * @param context Context.
 *
 * @return Existing Godot Error value.
 */
typedef int32_t (*GDExtensionInterfaceDeviceExitAfterUserTerminatorV1)(const GDExtensionDeviceExitAbi *abi, GDExtensionDeviceExitDeinitLevelContextRef context);

/**
 * @name device_exit_after_binding_deinit_v1
 * @since 4.7
 *
 * Internal reciprocal guarded-lifetime v1 surface. Availability does not enable guarded loading; the complete loader, binding and entry census gate remains required.
 *
 * Native aligned, nonaliasing pointers are required. Abi uses version 1, sizeof(GDExtensionDeviceExitAbi), reserved zero; embedded registration and diagnostic headers use their own generated sizeof.
 * Borrowed contexts and cleanup handles expire at the exact issuing callback return. The guard token lasts until enrollment revocation. Code holds persist across frames and retire after release at the original outer callback return. Empty outputs remain empty on failure; nonempty outputs are rejected unchanged.
 *
 * @param abi Abi.
 * @param context Context.
 *
 * @return Existing Godot Error value.
 */
typedef int32_t (*GDExtensionInterfaceDeviceExitAfterBindingDeinitV1)(const GDExtensionDeviceExitAbi *abi, GDExtensionDeviceExitDeinitLevelContextRef context);

/**
 * @name device_exit_publish_exit_result_v1
 * @since 4.7
 *
 * Internal reciprocal guarded-lifetime v1 surface. Availability does not enable guarded loading; the complete loader, binding and entry census gate remains required.
 *
 * Native aligned, nonaliasing pointers are required. Abi uses version 1, sizeof(GDExtensionDeviceExitAbi), reserved zero; embedded registration and diagnostic headers use their own generated sizeof.
 * Borrowed contexts and cleanup handles expire at the exact issuing callback return. The guard token lasts until enrollment revocation. Code holds persist across frames and retire after release at the original outer callback return. Empty outputs remain empty on failure; nonempty outputs are rejected unchanged.
 *
 * @param abi Abi.
 * @param token Token.
 * @param context Context.
 * @param diagnostic Diagnostic.
 *
 * @return Existing Godot Error value.
 */
typedef int32_t (*GDExtensionInterfaceDeviceExitPublishExitResultV1)(const GDExtensionDeviceExitAbi *abi, GDExtensionDeviceExitGuardToken token, GDExtensionDeviceExitDeinitPreflightContextRef context, const GDExtensionDeviceExitExitDiagnosticV1 *diagnostic);

/**
 * @name device_exit_terminate_initialize_blocked_v1
 * @since 4.7
 *
 * Internal reciprocal guarded-lifetime v1 surface. Availability does not enable guarded loading; the complete loader, binding and entry census gate remains required.
 *
 * Native aligned, nonaliasing pointers are required. Abi uses version 1, sizeof(GDExtensionDeviceExitAbi), reserved zero; embedded registration and diagnostic headers use their own generated sizeof.
 * Borrowed contexts and cleanup handles expire at the exact issuing callback return. The guard token lasts until enrollment revocation. Code holds persist across frames and retire after release at the original outer callback return. Empty outputs remain empty on failure; nonempty outputs are rejected unchanged.
 * Never returns, including for invalid context. Uses the original fixed core failure route.
 *
 * @param context Context.
 * @param original_error Original error.
 */
typedef void (*GDExtensionInterfaceDeviceExitTerminateInitializeBlockedV1)(GDExtensionDeviceExitInitializationLevelContextRef context, int32_t original_error);

/**
 * @name device_exit_terminate_blocked_v1
 * @since 4.7
 *
 * Internal reciprocal guarded-lifetime v1 surface. Availability does not enable guarded loading; the complete loader, binding and entry census gate remains required.
 *
 * Native aligned, nonaliasing pointers are required. Abi uses version 1, sizeof(GDExtensionDeviceExitAbi), reserved zero; embedded registration and diagnostic headers use their own generated sizeof.
 * Borrowed contexts and cleanup handles expire at the exact issuing callback return. The guard token lasts until enrollment revocation. Code holds persist across frames and retire after release at the original outer callback return. Empty outputs remain empty on failure; nonempty outputs are rejected unchanged.
 * Never returns, including for invalid context. Uses the original fixed core failure route.
 *
 * @param context Context.
 * @param original_error Original error.
 */
typedef void (*GDExtensionInterfaceDeviceExitTerminateBlockedV1)(GDExtensionDeviceExitDeinitLevelContextRef context, int32_t original_error);

/**
 * @name device_session_bind_v1
 * @since 4.7
 *
 * Candidate original RD session interface; authenticated guarded initialization binding.
 *
 * @param abi abi
 * @param frame frame
 * @param rd rd
 * @param out_context out_context
 * @param out_session out_session
 *
 * @return Native Error plus OI-01 reason, expected and actual. Reasons: 0 success, 1 source_version, 2 canonical, 3 nonzero, 4 context_present, 5 context_match, 6 device_epoch, 7 registered, 8 retained, 9 association_match, 10 authorized_phase, 11 capacity, 12 session_counter_available, 13 epoch_counter_available.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionBindV1)(const GDExtensionDeviceExitAbi *abi, GDExtensionDeviceExitInitializationLevelContextRef frame, GDExtensionObjectPtr rd, GDExtensionDeviceRegistryContextRef *out_context, GDExtensionOriginalDeviceSessionRef *out_session);

/**
 * @name device_session_copy_v1
 * @since 4.7
 *
 * Candidate original RD session interface; authenticated guarded initialization binding.
 *
 * @param context context
 * @param session session
 * @param exporting exporting
 * @param out_view out_view
 *
 * @return Native Error plus OI-01 reason, expected and actual. Reasons: 0 success, 1 source_version, 2 canonical, 3 nonzero, 4 context_present, 5 context_match, 6 device_epoch, 7 registered, 8 retained, 9 association_match, 10 authorized_phase, 11 capacity, 12 session_counter_available, 13 epoch_counter_available.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionCopyV1)(GDExtensionDeviceRegistryContextRef context, GDExtensionOriginalDeviceSessionRef session, uint32_t exporting, GDExtensionDeviceSessionViewV1 *out_view);

/**
 * @name device_session_resolve_v1
 * @since 4.7
 *
 * Candidate original RD session interface; authenticated guarded initialization binding.
 *
 * @param context context
 * @param epoch epoch
 * @param session session
 * @param out_session out_session
 *
 * @return Native Error plus OI-01 reason, expected and actual. Reasons: 0 success, 1 source_version, 2 canonical, 3 nonzero, 4 context_present, 5 context_match, 6 device_epoch, 7 registered, 8 retained, 9 association_match, 10 authorized_phase, 11 capacity, 12 session_counter_available, 13 epoch_counter_available.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionResolveV1)(GDExtensionDeviceRegistryContextRef context, uint64_t epoch, uint64_t session, GDExtensionOriginalDeviceSessionRef *out_session);

/**
 * @name device_session_release_context_v1
 * @since 4.7
 *
 * Candidate original RD session interface; authenticated guarded initialization binding.
 *
 * @param context context
 *
 * @return Native Error plus OI-01 reason, expected and actual. Reasons: 0 success, 1 source_version, 2 canonical, 3 nonzero, 4 context_present, 5 context_match, 6 device_epoch, 7 registered, 8 retained, 9 association_match, 10 authorized_phase, 11 capacity, 12 session_counter_available, 13 epoch_counter_available.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionReleaseContextV1)(GDExtensionDeviceRegistryContextRef context);

/**
 * @name device_session_release_session_v1
 * @since 4.7
 *
 * Candidate original RD session interface; authenticated guarded initialization binding.
 *
 * @param session session
 *
 * @return Native Error plus OI-01 reason, expected and actual. Reasons: 0 success, 1 source_version, 2 canonical, 3 nonzero, 4 context_present, 5 context_match, 6 device_epoch, 7 registered, 8 retained, 9 association_match, 10 authorized_phase, 11 capacity, 12 session_counter_available, 13 epoch_counter_available.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionReleaseSessionV1)(GDExtensionOriginalDeviceSessionRef session);

/**
 * @name device_exit_register_scene_ready_v1
 * @since 4.7
 *
 * Register the original generation scene-ready callback inside its qualified SCENE initializer before binding acknowledgement. Guarded lookup only; no late effects or enrollment are inferred.
 *
 * @param abi abi
 * @param original_scene_frame original_scene_frame
 * @param callback callback
 * @param original_userdata original_userdata
 *
 * @return Original Godot Error observation; no inferred enrollment or completion.
 */
typedef int32_t (*GDExtensionInterfaceDeviceExitRegisterSceneReadyV1)(const GDExtensionDeviceExitAbi *abi, GDExtensionDeviceExitInitializationLevelContextRef original_scene_frame, GDExtensionDeviceExitSceneReadyCallbackV1 callback, void *original_userdata);

/**
 * @name device_exit_register_scene_ready_v2
 * @since 4.7
 *
 * Register one original scene-ready transaction with fixed BaseScene/RDPair requirements. The original Registry selects and borrows a same-frame plan before callback; this does not issue a GPU capability. Guarded lookup only; v1 remains strict full installation.
 *
 * @param requirements requirements
 * @param original_scene_frame original_scene_frame
 * @param callback callback
 * @param original_userdata original_userdata
 *
 * @return Original Godot Error; malformed requirements fail before any record mutation.
 */
typedef int32_t (*GDExtensionInterfaceDeviceExitRegisterSceneReadyV2)(const GDExtensionSceneInstallRequirementsV2 *requirements, GDExtensionDeviceExitInitializationLevelContextRef original_scene_frame, GDExtensionDeviceExitSceneReadyCallbackV2 callback, void *original_userdata);

/**
 * @name device_exit_scene_ready_schedule_object_cleanup_v1
 * @since 4.7
 *
 * Schedule the actual same-generation cleanup and binding pair onto its already-issued original SCENE USER (0) or BINDING (1) action from the admitted ready frame.
 *
 * @param abi abi
 * @param ready ready
 * @param phase phase
 * @param original_object_id original_object_id
 *
 * @return Original Godot Error observation; no inferred enrollment or completion.
 */
typedef int32_t (*GDExtensionInterfaceDeviceExitSceneReadyScheduleObjectCleanupV1)(const GDExtensionDeviceExitAbi *abi, GDExtensionDeviceExitSceneReadyContextRef ready, uint32_t phase, uint64_t original_object_id);

/**
 * @name device_session_bind_scene_ready_v1
 * @since 4.7
 *
 * Bind the actual original native RD record from the admitted scene-ready frame using the original atomic two-reference session transaction.
 *
 * @param abi abi
 * @param ready ready
 * @param original_rd original_rd
 * @param out_context out_context
 * @param out_session out_session
 *
 * @return Original Godot Error observation; no inferred enrollment or completion.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionBindSceneReadyV1)(const GDExtensionDeviceExitAbi *abi, GDExtensionDeviceExitSceneReadyContextRef ready, GDExtensionObjectPtr original_rd, GDExtensionDeviceRegistryContextRef *out_context, GDExtensionOriginalDeviceSessionRef *out_session);

/**
 * @name device_exit_scene_ready_abort_v1
 * @since 4.7
 *
 * Validate the original admitted ready frame and exact unscheduled object pair, then perform the fixed non-returning core stop before owner unwinding. Malformed direct calls return Error without side effects.
 *
 * @param abi abi
 * @param ready ready
 * @param original_schedule_error original_schedule_error
 * @param reason reason
 * @param original_object_id original_object_id
 *
 * @return Original Godot Error observation; no inferred enrollment or completion.
 */
typedef int32_t (*GDExtensionInterfaceDeviceExitSceneReadyAbortV1)(const GDExtensionDeviceExitAbi *abi, GDExtensionDeviceExitSceneReadyContextRef ready, int32_t original_schedule_error, uint32_t reason, uint64_t original_object_id);

/**
 * @name device_session_bind_main_scene_ready_v1
 * @since 4.7
 *
 * Original-bridge qualification ABI declaration; availability requires the actual guarded implementation. Copied correlation is not recording, completion, or disposal authority.
 *
 * @param abi Synchronous in-process abi.
 * @param ready Synchronous in-process ready.
 * @param out_context Synchronous in-process out context.
 * @param out_session Synchronous in-process out session.
 * @param out_main_object_id Synchronous in-process out main object id.
 *
 * @return Original native Error/reason observation. A possible effect requires a retained original child/ticket, never an unowned bare error.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionBindMainSceneReadyV1)(const GDExtensionDeviceExitAbi *abi, GDExtensionDeviceExitSceneReadyContextRef ready, GDExtensionDeviceRegistryContextRef *out_context, GDExtensionOriginalDeviceSessionRef *out_session, uint64_t *out_main_object_id);

/**
 * @name device_session_qualification_open_v1
 * @since 4.7
 *
 * Original-bridge qualification ABI declaration; availability requires the actual guarded implementation. Copied correlation is not recording, completion, or disposal authority.
 *
 * @param abi Synchronous in-process abi.
 * @param ready Synchronous in-process ready.
 * @param context Synchronous in-process context.
 * @param session Synchronous in-process session.
 * @param code_hold Synchronous in-process code hold.
 * @param out_bridge Synchronous in-process out bridge.
 *
 * @return Original native Error/reason observation. A possible effect requires a retained original child/ticket, never an unowned bare error.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationOpenV1)(const GDExtensionDeviceExitAbi *abi, GDExtensionDeviceExitSceneReadyContextRef ready, GDExtensionDeviceRegistryContextRef context, GDExtensionOriginalDeviceSessionRef session, GDExtensionDeviceExitLibraryCodeHoldRef code_hold, GDExtensionQualificationBridgeRefV1 *out_bridge);

/**
 * @name device_session_qualification_backing_v1
 * @since 4.7
 *
 * Original-bridge qualification ABI declaration; availability requires the actual guarded implementation. Copied correlation is not recording, completion, or disposal authority.
 *
 * @param bridge Synchronous in-process bridge.
 * @param rid Synchronous in-process rid.
 * @param out_backing Synchronous in-process out backing.
 *
 * @return Original native Error/reason observation. A possible effect requires a retained original child/ticket, never an unowned bare error.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationBackingV1)(GDExtensionQualificationBridgeRefV1 bridge, uint64_t rid, GDExtensionQualificationBackingV1 *out_backing);

/**
 * @name device_session_qualification_record_v1
 * @since 4.7
 *
 * Original-bridge qualification ABI declaration; availability requires the actual guarded implementation. Copied correlation is not recording, completion, or disposal authority.
 *
 * @param bridge Synchronous in-process bridge.
 * @param request Synchronous in-process request.
 * @param out_ticket Synchronous in-process out ticket.
 * @param out_report Synchronous in-process out report.
 *
 * @return Original native Error/reason observation. A possible effect requires a retained original child/ticket, never an unowned bare error.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationRecordV1)(GDExtensionQualificationBridgeRefV1 bridge, const GDExtensionQualificationRecordV1 *request, GDExtensionQualificationTicketRefV1 *out_ticket, GDExtensionQualificationReportV1 *out_report);

/**
 * @name device_session_qualification_observe_v1
 * @since 4.7
 *
 * Original-bridge qualification ABI declaration; availability requires the actual guarded implementation. Copied correlation is not recording, completion, or disposal authority.
 *
 * @param bridge Synchronous in-process bridge.
 * @param ticket Synchronous in-process ticket.
 * @param budget Synchronous in-process budget.
 * @param out_report Synchronous in-process out report.
 *
 * @return Original native Error/reason observation. A possible effect requires a retained original child/ticket, never an unowned bare error.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationObserveV1)(GDExtensionQualificationBridgeRefV1 bridge, GDExtensionQualificationTicketRefV1 ticket, const GDExtensionQualificationBudgetV1 *budget, GDExtensionQualificationReportV1 *out_report);

/**
 * @name device_session_qualification_facts_v1
 * @since 4.7
 *
 * Original-bridge qualification ABI declaration; availability requires the actual guarded implementation. Copied correlation is not recording, completion, or disposal authority.
 *
 * @param bridge Synchronous in-process bridge.
 * @param ticket Synchronous in-process ticket.
 * @param revision Synchronous in-process revision.
 * @param first Synchronous in-process first.
 * @param capacity Synchronous in-process capacity.
 * @param out_facts Synchronous in-process out facts.
 * @param out_written Synchronous in-process out written.
 *
 * @return Original native Error/reason observation. A possible effect requires a retained original child/ticket, never an unowned bare error.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationFactsV1)(GDExtensionQualificationBridgeRefV1 bridge, GDExtensionQualificationTicketRefV1 ticket, uint64_t revision, uint32_t first, uint32_t capacity, GDExtensionQualificationFactV1 *out_facts, uint32_t *out_written);

/**
 * @name device_session_qualification_copy_v1
 * @since 4.7
 *
 * Original-bridge qualification ABI declaration; availability requires the actual guarded implementation. Copied correlation is not recording, completion, or disposal authority.
 *
 * @param bridge Synchronous in-process bridge.
 * @param ticket Synchronous in-process ticket.
 * @param revision Synchronous in-process revision.
 * @param destination Synchronous in-process destination.
 * @param capacity Synchronous in-process capacity.
 * @param out_written Synchronous in-process out written.
 *
 * @return Original native Error/reason observation. A possible effect requires a retained original child/ticket, never an unowned bare error.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationCopyV1)(GDExtensionQualificationBridgeRefV1 bridge, GDExtensionQualificationTicketRefV1 ticket, uint64_t revision, void *destination, uint32_t capacity, uint32_t *out_written);

/**
 * @name device_session_qualification_release_v1
 * @since 4.7
 *
 * Original-bridge qualification ABI declaration; availability requires the actual guarded implementation. Copied correlation is not recording, completion, or disposal authority.
 *
 * @param bridge Synchronous in-process bridge.
 * @param ticket Synchronous in-process ticket.
 *
 * @return Original native Error/reason observation. A possible effect requires a retained original child/ticket, never an unowned bare error.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationReleaseV1)(GDExtensionQualificationBridgeRefV1 bridge, GDExtensionQualificationTicketRefV1 ticket);

/**
 * @name device_session_qualification_close_v1
 * @since 4.7
 *
 * Original-bridge qualification ABI declaration; availability requires the actual guarded implementation. Copied correlation is not recording, completion, or disposal authority.
 *
 * @param bridge Synchronous in-process bridge.
 *
 * @return Original native Error/reason observation. A possible effect requires a retained original child/ticket, never an unowned bare error.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationCloseV1)(GDExtensionQualificationBridgeRefV1 bridge);

/**
 * @name device_session_qualification_storage_create_v1
 * @since 4.7
 *
 * Original-bridge qualification ABI declaration; availability requires the actual guarded implementation. Copied correlation is not recording, completion, or disposal authority.
 *
 * @param bridge Synchronous in-process bridge.
 * @param request Synchronous in-process request.
 * @param out_backing Synchronous in-process out backing.
 *
 * @return Original native Error/reason observation. A possible effect requires a retained original child/ticket, never an unowned bare error.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationStorageCreateV1)(GDExtensionQualificationBridgeRefV1 bridge, const GDExtensionStorageCreateRequestV1 *request, GDExtensionStorageBackingV1 *out_backing);

/**
 * @name device_session_qualification_storage_retire_v1
 * @since 4.7
 *
 * Original-bridge qualification ABI declaration; availability requires the actual guarded implementation. Copied correlation is not recording, completion, or disposal authority.
 *
 * @param bridge Synchronous in-process bridge.
 * @param request Synchronous in-process request.
 * @param out_retire Synchronous in-process out retire.
 * @param out_report Synchronous in-process out report.
 *
 * @return Original native Error/reason observation. A possible effect requires a retained original child/ticket, never an unowned bare error.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationStorageRetireV1)(GDExtensionQualificationBridgeRefV1 bridge, const GDExtensionStorageRetireRequestV1 *request, GDExtensionStorageRetireRefV1 *out_retire, GDExtensionStorageRetireReportV1 *out_report);

/**
 * @name device_session_qualification_storage_retire_observe_v1
 * @since 4.7
 *
 * Original-bridge qualification ABI declaration; availability requires the actual guarded implementation. Copied correlation is not recording, completion, or disposal authority.
 *
 * @param bridge Synchronous in-process bridge.
 * @param retire Synchronous in-process retire.
 * @param budget Synchronous in-process budget.
 * @param out_report Synchronous in-process out report.
 *
 * @return Original native Error/reason observation. A possible effect requires a retained original child/ticket, never an unowned bare error.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationStorageRetireObserveV1)(GDExtensionQualificationBridgeRefV1 bridge, GDExtensionStorageRetireRefV1 retire, const GDExtensionQualificationBudgetV1 *budget, GDExtensionStorageRetireReportV1 *out_report);

/**
 * @name device_session_qualification_storage_retire_release_v1
 * @since 4.7
 *
 * Original-bridge qualification ABI declaration; availability requires the actual guarded implementation. Copied correlation is not recording, completion, or disposal authority.
 *
 * @param bridge Synchronous in-process bridge.
 * @param retire Synchronous in-process retire.
 *
 * @return Original native Error/reason observation. A possible effect requires a retained original child/ticket, never an unowned bare error.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationStorageRetireReleaseV1)(GDExtensionQualificationBridgeRefV1 bridge, GDExtensionStorageRetireRefV1 retire);

/**
 * @name device_session_qualification_prepare_uploads_v1
 * @since 4.7
 *
 * Original-bridge qualification ABI declaration; availability requires the actual guarded implementation. Copied correlation is not recording, completion, or disposal authority.
 *
 * @param bridge Synchronous in-process bridge.
 * @param request Synchronous in-process request.
 * @param plans Synchronous in-process plans.
 * @param plan_count Synchronous in-process plan count.
 * @param out_ticket Synchronous in-process out ticket.
 * @param out_report Synchronous in-process out report.
 *
 * @return Original native Error/reason observation. A possible effect requires a retained original child/ticket, never an unowned bare error.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationPrepareUploadsV1)(GDExtensionQualificationBridgeRefV1 bridge, const GDExtensionQualificationUploadPrepareV1 *request, const GDExtensionQualificationUploadPlanV1 *plans, uint32_t plan_count, GDExtensionQualificationTicketRefV1 *out_ticket, GDExtensionQualificationReportV1 *out_report);

/**
 * @name device_session_qualification_upload_v1
 * @since 4.7
 *
 * Original-bridge qualification ABI declaration; availability requires the actual guarded implementation. Copied correlation is not recording, completion, or disposal authority.
 *
 * @param bridge Synchronous in-process bridge.
 * @param ticket Synchronous in-process ticket.
 * @param request Synchronous in-process request.
 * @param owned_bytes Synchronous in-process owned bytes.
 * @param byte_count Synchronous in-process byte count.
 * @param out_segments Synchronous in-process out segments.
 * @param segment_capacity Synchronous in-process segment capacity.
 * @param out_report Synchronous in-process out report.
 *
 * @return Original native Error/reason observation. A possible effect requires a retained original child/ticket, never an unowned bare error.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationUploadV1)(GDExtensionQualificationBridgeRefV1 bridge, GDExtensionQualificationTicketRefV1 ticket, const GDExtensionQualificationUploadRequestV1 *request, const void *owned_bytes, uint32_t byte_count, GDExtensionQualificationUploadSegmentV1 *out_segments, uint32_t segment_capacity, GDExtensionQualificationUploadReportV1 *out_report);

/**
 * @name device_session_qualification_record_prepared_v1
 * @since 4.7
 *
 * Original-bridge qualification ABI declaration; availability requires the actual guarded implementation. Copied correlation is not recording, completion, or disposal authority.
 *
 * @param bridge Synchronous in-process bridge.
 * @param ticket Synchronous in-process ticket.
 * @param request Synchronous in-process request.
 * @param out_report Synchronous in-process out report.
 *
 * @return Original native Error/reason observation. A possible effect requires a retained original child/ticket, never an unowned bare error.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationRecordPreparedV1)(GDExtensionQualificationBridgeRefV1 bridge, GDExtensionQualificationTicketRefV1 ticket, const GDExtensionQualificationRecordV1 *request, GDExtensionQualificationReportV1 *out_report);

/**
 * @name device_session_qualification_native_unit_create_v1
 * @since 4.7
 *
 * Original-bridge qualification ABI declaration; availability requires the actual guarded implementation. Copied correlation is not recording, completion, or disposal authority.
 *
 * @param bridge Synchronous in-process bridge.
 * @param request Synchronous in-process request.
 * @param container_bytes Synchronous in-process container bytes.
 * @param byte_count Synchronous in-process byte count.
 * @param out_backing Synchronous in-process out backing.
 *
 * @return Original native Error/reason observation. A possible effect requires a retained original child/ticket, never an unowned bare error.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationNativeUnitCreateV1)(GDExtensionQualificationBridgeRefV1 bridge, const GDExtensionNativeUnitCreateRequestV1 *request, const void *container_bytes, uint64_t byte_count, GDExtensionNativeUnitBackingV1 *out_backing);

/**
 * @name device_session_qualification_native_unit_retire_v1
 * @since 4.7
 *
 * Original-bridge qualification ABI declaration; availability requires the actual guarded implementation. Copied correlation is not recording, completion, or disposal authority.
 *
 * @param bridge Synchronous in-process bridge.
 * @param request Synchronous in-process request.
 * @param out_retire Synchronous in-process out retire.
 * @param out_report Synchronous in-process out report.
 *
 * @return Original native Error/reason observation. A possible effect requires a retained original child/ticket, never an unowned bare error.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationNativeUnitRetireV1)(GDExtensionQualificationBridgeRefV1 bridge, const GDExtensionNativeUnitRetireRequestV1 *request, GDExtensionNativeUnitRetireRefV1 *out_retire, GDExtensionNativeUnitRetireReportV1 *out_report);

/**
 * @name device_session_qualification_native_unit_retire_observe_v1
 * @since 4.7
 *
 * Original-bridge qualification ABI declaration; availability requires the actual guarded implementation. Copied correlation is not recording, completion, or disposal authority.
 *
 * @param bridge Synchronous in-process bridge.
 * @param retire Synchronous in-process retire.
 * @param budget Synchronous in-process budget.
 * @param out_report Synchronous in-process out report.
 *
 * @return Original native Error/reason observation. A possible effect requires a retained original child/ticket, never an unowned bare error.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationNativeUnitRetireObserveV1)(GDExtensionQualificationBridgeRefV1 bridge, GDExtensionNativeUnitRetireRefV1 retire, const GDExtensionQualificationBudgetV1 *budget, GDExtensionNativeUnitRetireReportV1 *out_report);

/**
 * @name device_session_qualification_native_unit_retire_release_v1
 * @since 4.7
 *
 * Original-bridge qualification ABI declaration; availability requires the actual guarded implementation. Copied correlation is not recording, completion, or disposal authority.
 *
 * @param bridge Synchronous in-process bridge.
 * @param retire Synchronous in-process retire.
 *
 * @return Original native Error/reason observation. A possible effect requires a retained original child/ticket, never an unowned bare error.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationNativeUnitRetireReleaseV1)(GDExtensionQualificationBridgeRefV1 bridge, GDExtensionNativeUnitRetireRefV1 retire);

/**
 * @name device_session_qualification_cancel_v1
 * @since 4.7
 *
 * Stops future upload or prepared-record admission on the same original opt-in ticket. It does not acknowledge or discard physical work.
 *
 * @param bridge Synchronous in-process bridge.
 * @param ticket Synchronous in-process ticket.
 *
 * @return Original native Error/reason observation. A possible effect requires a retained original child/ticket, never an unowned bare error.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationCancelV1)(GDExtensionQualificationBridgeRefV1 bridge, GDExtensionQualificationTicketRefV1 ticket);

/**
 * @name device_session_qualification_rt_configure_v1
 * @since 4.7
 *
 * Original bridge fixed typed owner-to-main-RT transport; a returned call is not native completion or physical retirement.
 *
 * @param bridge Bounded synchronous bridge value or span; no caller pointer is retained.
 * @param config Bounded synchronous config value or span; no caller pointer is retained.
 *
 * @return Original native Error and closed device-session reason; API call return is not GPU completion.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationRtConfigureV1)(GDExtensionQualificationBridgeRefV1 bridge, const GDExtensionQualificationRtConfigV1 *config);

/**
 * @name device_session_qualification_rt_reserve_v1
 * @since 4.7
 *
 * Original bridge fixed typed owner-to-main-RT transport; a returned call is not native completion or physical retirement.
 *
 * @param bridge Bounded synchronous bridge value or span; no caller pointer is retained.
 * @param header Bounded synchronous header value or span; no caller pointer is retained.
 * @param request_pod Bounded synchronous request_pod value or span; no caller pointer is retained.
 * @param payload Bounded synchronous payload value or span; no caller pointer is retained.
 * @param out_ref Bounded synchronous out_ref value or span; no caller pointer is retained.
 *
 * @return Original native Error and closed device-session reason; API call return is not GPU completion.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationRtReserveV1)(GDExtensionQualificationBridgeRefV1 bridge, const GDExtensionQualificationRtRequestV1 *header, const void *request_pod, const void *payload, GDExtensionQualificationRtRefV1 *out_ref);

/**
 * @name device_session_qualification_rt_activate_v1
 * @since 4.7
 *
 * Original bridge fixed typed owner-to-main-RT transport; a returned call is not native completion or physical retirement.
 *
 * @param bridge Bounded synchronous bridge value or span; no caller pointer is retained.
 * @param ref Bounded synchronous ref value or span; no caller pointer is retained.
 *
 * @return Original native Error and closed device-session reason; API call return is not GPU completion.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationRtActivateV1)(GDExtensionQualificationBridgeRefV1 bridge, GDExtensionQualificationRtRefV1 ref);

/**
 * @name device_session_qualification_rt_observe_v1
 * @since 4.7
 *
 * Original bridge fixed typed owner-to-main-RT transport; a returned call is not native completion or physical retirement.
 *
 * @param bridge Bounded synchronous bridge value or span; no caller pointer is retained.
 * @param ref Bounded synchronous ref value or span; no caller pointer is retained.
 * @param out_observation Bounded synchronous out_observation value or span; no caller pointer is retained.
 * @param out_result Bounded synchronous out_result value or span; no caller pointer is retained.
 * @param result_capacity Bounded synchronous result_capacity value or span; no caller pointer is retained.
 * @param out_segments Bounded synchronous out_segments value or span; no caller pointer is retained.
 * @param segment_capacity Bounded synchronous segment_capacity value or span; no caller pointer is retained.
 *
 * @return Original native Error and closed device-session reason; API call return is not GPU completion.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationRtObserveV1)(GDExtensionQualificationBridgeRefV1 bridge, GDExtensionQualificationRtRefV1 ref, GDExtensionQualificationRtObservationV1 *out_observation, void *out_result, uint32_t result_capacity, GDExtensionQualificationUploadSegmentV1 *out_segments, uint32_t segment_capacity);

/**
 * @name device_session_qualification_rt_release_v1
 * @since 4.7
 *
 * Original bridge fixed typed owner-to-main-RT transport; a returned call is not native completion or physical retirement.
 *
 * @param bridge Bounded synchronous bridge value or span; no caller pointer is retained.
 * @param ref Bounded synchronous ref value or span; no caller pointer is retained.
 *
 * @return Original native Error and closed device-session reason; API call return is not GPU completion.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationRtReleaseV1)(GDExtensionQualificationBridgeRefV1 bridge, GDExtensionQualificationRtRefV1 ref);

#ifdef __cplusplus
}
#endif
