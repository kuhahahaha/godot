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

/* Opaque retained original Host producer registration. Numbers are handles only; labels do not authenticate a registration. This is not a code hold or operation/source grant. */
typedef struct {
	uint64_t registry_instance;
	uint64_t handle_serial;
} GDExtensionProducerRegistrationRefV1;

/* Read-only original registration identity. Native ABI version 1, struct_size sizeof this structure, reserved zero. label is bounded NUL-terminated UTF-8; copying it does not retain or authenticate the producer. */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t registry_instance;
	uint64_t library_generation;
	char label[96];
} GDExtensionProducerRegistrationIdentityV1;

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

typedef enum {
	GDEXTENSION_QUALIFICATION_READBACK_NUMERIC_RAW_OUTPUT_V1 = 0,
	GDEXTENSION_QUALIFICATION_READBACK_METHOD_RAW_RESULTS_V1 = 1,
	GDEXTENSION_QUALIFICATION_READBACK_METHOD_DESTINATION_V1 = 2,
	GDEXTENSION_QUALIFICATION_READBACK_METHOD_FAULT_PLANE_V1 = 3,
	GDEXTENSION_QUALIFICATION_READBACK_METHOD_RESULT_EPILOGUE_V1 = 4,
	GDEXTENSION_QUALIFICATION_READBACK_METHOD_INPUT_GATE_V1 = 5,
	GDEXTENSION_QUALIFICATION_READBACK_METHOD_INSTANCE_GATE_V1 = 6,
	GDEXTENSION_QUALIFICATION_READBACK_METHOD_OUTPUT_GATE_V1 = 7,
	GDEXTENSION_QUALIFICATION_READBACK_ROLE_COUNT_V1 = 8,
} GDExtensionQualificationReadbackRoleV1;

typedef struct {
	uint64_t readback_buffer;
	uint64_t source_allocation_generation;
	uint64_t observed_backing_serial;
	uint64_t readback_offset;
	uint64_t readback_size;
	uint32_t result_offset;
	uint32_t role;
} GDExtensionQualificationReadbackRangeV1;

typedef struct {
	GDExtensionQualificationReadbackRangeV1 source;
	uint64_t readback_request;
} GDExtensionQualificationReadbackObservationV1;

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
	uint64_t capture_set1;
	uint64_t body_set1;
	uint32_t capture_set_count;
	uint32_t body_set_count;
	uint32_t readback_count;
	uint32_t readback_size;
	GDExtensionQualificationReadbackRangeV1 readbacks[3];
	uint8_t capture_push[16];
	uint8_t body_push[16];
} GDExtensionQualificationRecordV2;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionQualificationRecordV2 record;
	uint64_t original_range_begin;
	uint64_t original_range_end;
} GDExtensionQualificationRecordV3;

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
	uint32_t readback_count;
	uint32_t readback_size;
	GDExtensionQualificationReadbackObservationV1 readbacks[3];
	uint32_t owned_size;
	uint32_t reserved;
	uint8_t request_digest[32];
	uint8_t unit_digest[32];
} GDExtensionQualificationReportV2;

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
	/* StorageCreate only: ERR_BUSY after the exact original-origin scan found no existing child, before this call allocated a child, serial or cleanup credit or entered native allocation. Backing and expected/actual remain zero.
	 * Does not certify future admission, absence of later concurrent calls, GPU completion, original account refund or permission to create after close. Existing creating prefixes, bridge-level refusal and non-Busy errors retain their existing reasons.
	 */
	GDEXTENSION_DEVICE_SESSION_REASON_STORAGE_CREATE_DEFERRED_V1 = 20,
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
	GDEXTENSION_QUALIFICATION_UPLOAD_ROLE_METHOD_DOMAIN_COUNT_V1 = 8,
	GDEXTENSION_QUALIFICATION_UPLOAD_ROLE_METHOD_FAULT_PLANE_V1 = 9,
	GDEXTENSION_QUALIFICATION_UPLOAD_ROLE_METHOD_INPUT_GATE_V1 = 10,
	GDEXTENSION_QUALIFICATION_UPLOAD_ROLE_METHOD_INSTANCE_GATE_V1 = 11,
	GDEXTENSION_QUALIFICATION_UPLOAD_ROLE_METHOD_MAPPING_V1 = 12,
	GDEXTENSION_QUALIFICATION_UPLOAD_ROLE_METHOD_OUTPUT_GATE_V1 = 13,
	GDEXTENSION_QUALIFICATION_UPLOAD_ROLE_METHOD_RAW_PARAMETERS_V1 = 14,
	GDEXTENSION_QUALIFICATION_UPLOAD_ROLE_METHOD_RAW_RESULTS_V1 = 15,
	GDEXTENSION_QUALIFICATION_UPLOAD_ROLE_METHOD_RESULT_EPILOGUE_V1 = 16,
	GDEXTENSION_QUALIFICATION_UPLOAD_ROLE_METHOD_INVOCATION_INPUTS_V1 = 17,
	GDEXTENSION_QUALIFICATION_UPLOAD_ROLE_METHOD_RESOURCE_V1 = 18,
	GDEXTENSION_QUALIFICATION_UPLOAD_ROLE_COUNT_V1 = 19,
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

typedef struct {
	uint64_t session_serial;
	uint64_t generation;
	uint8_t digest[32];
} GDExtensionQualificationSourceV1;

typedef struct {
	uint64_t registry_instance;
	uint64_t handle_serial;
} GDExtensionQualificationResourceRefV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionQualificationResourceRefV1 resource;
	GDExtensionQualificationSourceV1 source;
	uint64_t rid;
	uint64_t object_serial;
	uint64_t byte_size;
	uint32_t kind;
	uint32_t state;
} GDExtensionQualificationResourceBackingV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionQualificationSourceV1 source;
	uint64_t library_generation;
	uint64_t create_attempt;
	uint32_t width;
	uint32_t height;
	uint32_t depth;
	uint32_t texture_type;
	uint32_t format;
	uint32_t usage;
} GDExtensionQualificationTextureCreateV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionQualificationSourceV1 source;
	uint64_t library_generation;
	uint64_t create_attempt;
	uint32_t mag_filter;
	uint32_t min_filter;
	uint32_t mip_filter;
	uint32_t repeat_u;
	uint32_t repeat_v;
	uint32_t repeat_w;
} GDExtensionQualificationSamplerCreateV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionQualificationSourceV1 source;
	uint64_t library_generation;
	uint64_t create_attempt;
	GDExtensionQualificationTicketRefV1 ticket;
	GDExtensionNativeUnitRefV1 unit;
	uint64_t shader_rid;
	uint64_t shader_serial;
	uint32_t shader_set;
	uint32_t binding_count;
} GDExtensionQualificationSetCreateV1;

typedef struct {
	uint32_t binding;
	uint32_t type;
	GDExtensionQualificationResourceRefV1 resource;
	uint64_t rid;
	uint64_t object_serial;
	GDExtensionQualificationResourceRefV1 secondary;
	uint64_t secondary_rid;
	uint64_t secondary_serial;
} GDExtensionQualificationUniformBindingV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionQualificationTicketRefV1 ticket;
	GDExtensionQualificationResourceRefV1 texture;
	uint64_t record_generation;
	uint64_t parent_serial;
	uint64_t object_serial;
	uint64_t expected_revision;
	uint64_t byte_size;
	uint8_t payload_digest[32];
} GDExtensionQualificationTextureUploadV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionQualificationResourceRefV1 resource;
	uint64_t rid;
	uint64_t object_serial;
	uint64_t retire_operation;
} GDExtensionQualificationResourceRetireV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionQualificationResourceRefV1 resource;
	uint64_t rid;
	uint64_t object_serial;
	uint64_t retire_operation;
	uint64_t revision;
	uint64_t frame_epoch;
	uint64_t event_seq;
	uint64_t dependency_serial;
	uint32_t state;
	uint32_t return_status;
	uint32_t frame_slot;
	uint32_t reason;
	int32_t native_error;
	uint32_t kind;
	int64_t native_code;
} GDExtensionQualificationResourceRetireReportV1;

typedef enum {
	GDEXTENSION_QUALIFICATION_RESOURCE_INVALID_V1 = 0,
	GDEXTENSION_QUALIFICATION_RESOURCE_TEXTURE_V1 = 1,
	GDEXTENSION_QUALIFICATION_RESOURCE_SAMPLER_V1 = 2,
	GDEXTENSION_QUALIFICATION_RESOURCE_SET_V1 = 3,
} GDExtensionQualificationResourceKindV1;

typedef enum {
	GDEXTENSION_QUALIFICATION_TEXTURE_TYPE_INVALID_V1 = 0,
	GDEXTENSION_QUALIFICATION_TEXTURE_TYPE_1D_V1 = 1,
	GDEXTENSION_QUALIFICATION_TEXTURE_TYPE_2D_V1 = 2,
	GDEXTENSION_QUALIFICATION_TEXTURE_TYPE_3D_V1 = 3,
} GDExtensionQualificationTextureTypeV1;

typedef enum {
	GDEXTENSION_QUALIFICATION_TEXTURE_FORMAT_INVALID_V1 = 0,
	GDEXTENSION_QUALIFICATION_TEXTURE_FORMAT_RGBA32F_V1 = 1,
	GDEXTENSION_QUALIFICATION_TEXTURE_FORMAT_RGBA8_V1 = 2,
	GDEXTENSION_QUALIFICATION_TEXTURE_FORMAT_D32_SFLOAT_V1 = 3,
} GDExtensionQualificationTextureFormatV1;

typedef enum {
	GDEXTENSION_QUALIFICATION_BINDING_INVALID_V1 = 0,
	GDEXTENSION_QUALIFICATION_BINDING_STORAGE_BUFFER_V1 = 1,
	GDEXTENSION_QUALIFICATION_BINDING_TEXTURE_V1 = 2,
	GDEXTENSION_QUALIFICATION_BINDING_SAMPLER_V1 = 3,
	GDEXTENSION_QUALIFICATION_BINDING_SAMPLER_WITH_TEXTURE_V1 = 4,
	GDEXTENSION_QUALIFICATION_BINDING_IMAGE_V1 = 5,
} GDExtensionQualificationBindingTypeV1;

typedef struct {
	uint64_t registry_instance;
	uint64_t handle_serial;
} GDExtensionSystemScalarTicketRefV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionNativeUnitBackingV1 unit;
	GDExtensionStorageBackingV1 output;
	uint64_t output_allocation_generation;
	uint64_t library_generation;
	uint64_t session_serial;
	uint64_t record_generation;
	uint64_t request_nonce;
	uint64_t output_offset;
	uint32_t output_word_count;
	uint32_t reserved;
} GDExtensionSystemScalarRecordV1;

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
} GDExtensionSystemScalarReportV1;

/* Checked Core production physical transport. Availability is not original Runtime logical source, operation or window authority. */
typedef struct {
	uint64_t registry_instance;
	uint64_t handle_serial;
} GDExtensionProductionAttachmentRefV1;

/* Checked Core production physical transport. Availability is not original Runtime logical source, operation or window authority. */
typedef struct {
	uint64_t registry_instance;
	uint64_t handle_serial;
} GDExtensionProductionRequestRefV1;

/* Checked Core production physical transport. Availability is not original Runtime logical source, operation or window authority.
 * The abi header uses the original DeviceExit ABI version 1 and reserved zero; struct_size is sizeof(GDExtensionProductionCapacityV1), not sizeof(GDExtensionDeviceExitAbi). Header-size fallback is not supported.
 */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint32_t requests;
	uint32_t suboperations;
	uint32_t resources;
	uint32_t resource_uses;
	uint32_t graph_memberships;
	uint32_t native_memberships;
	uint32_t readbacks;
	uint32_t return_cells;
	uint64_t retained_payload_bytes;
	uint64_t readback_bytes;
} GDExtensionProductionCapacityV1;

/* Checked Core production physical transport. Availability is not original Runtime logical source, operation or window authority.
 * The abi header uses the original DeviceExit ABI version 1 and reserved zero; struct_size is sizeof(GDExtensionProductionMetadataQuoteV1), not sizeof(GDExtensionDeviceExitAbi). Header-size fallback is not supported.
 */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionProductionCapacityV1 capacity;
	uint64_t allocation_bytes;
	uint64_t allocation_alignment;
	uint64_t graph_slots;
	uint64_t graph_frames;
	uint64_t cleanup_slots;
} GDExtensionProductionMetadataQuoteV1;

/* Checked Core production physical transport. Availability is not original Runtime logical source, operation or window authority. */
typedef enum {
	GDEXTENSION_PRODUCTION_INSTALL_ACCEPTED_V1 = 1,
	GDEXTENSION_PRODUCTION_INSTALL_RETURN_PENDING_V1 = 2,
	GDEXTENSION_PRODUCTION_INSTALL_RETURNED_V1 = 3,
} GDExtensionProductionInstallationStateV1;

/* Checked Core production physical transport. Availability is not original Runtime logical source, operation or window authority.
 * The abi header uses the original DeviceExit ABI version 1 and reserved zero; struct_size is sizeof(GDExtensionProductionInstallationReportV1), not sizeof(GDExtensionDeviceExitAbi). Header-size fallback is not supported.
 */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint32_t state;
	uint32_t reserved;
	uint64_t allocated_bytes;
	uint64_t returned_bytes;
	GDExtensionDeviceSessionResultV1 cause;
} GDExtensionProductionInstallationReportV1;

/* Checked Core production physical transport. Availability is not original Runtime logical source, operation or window authority.
 * The abi header uses the original DeviceExit ABI version 1 and reserved zero; struct_size is sizeof(GDExtensionProductionOperationV1), not sizeof(GDExtensionDeviceExitAbi). Header-size fallback is not supported.
 */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t owner_request_nonce;
	uint64_t record_generation;
	uint64_t canonical_begin;
	uint64_t canonical_end;
} GDExtensionProductionOperationV1;

/* Checked Core production physical transport. Availability is not original Runtime logical source, operation or window authority. */
typedef enum {
	GDEXTENSION_PRODUCTION_DISPATCH_V1 = 1,
	GDEXTENSION_PRODUCTION_COPY_V1 = 2,
	GDEXTENSION_PRODUCTION_UPLOAD_V1 = 3,
	GDEXTENSION_PRODUCTION_READBACK_V1 = 4,
	GDEXTENSION_PRODUCTION_RASTER_FRAME_V1 = 5,
	GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1 = 6,
} GDExtensionProductionActionKindV1;

/* Checked Core production physical transport. Availability is not original Runtime logical source, operation or window authority. */
typedef enum {
	GDEXTENSION_PRODUCTION_STORAGE_V1 = 1,
	GDEXTENSION_PRODUCTION_UNIT_V1 = 2,
	GDEXTENSION_PRODUCTION_SET_V1 = 3,
	GDEXTENSION_PRODUCTION_RASTER_PHYSICAL_V1 = 4,
	GDEXTENSION_PRODUCTION_OUTPUT_METADATA_V1 = 5,
} GDExtensionProductionResourceKindV1;

/* Checked Core production raster transport. This independent versioned DTO does not reuse dispatch ordinals or qualification ownership. */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t action_ordinal;
	uint64_t canonical_suboperation;
	uint32_t framebuffer_resource_slot;
	uint32_t depth_resource_slot;
	uint64_t target_content_revision;
	uint64_t shader_serial;
	uint64_t pipeline_serial;
	uint64_t framebuffer_serial;
	uint32_t reserved;
} GDExtensionProductionRasterActionV1;

/* Checked Core production physical transport. Availability is not original Runtime logical source, operation or window authority. */
typedef enum {
	GDEXTENSION_PRODUCTION_READ_V1 = 1,
	GDEXTENSION_PRODUCTION_WRITE_V1 = 2,
	GDEXTENSION_PRODUCTION_READ_WRITE_V1 = 3,
	GDEXTENSION_PRODUCTION_ATOMIC_V1 = 4,
} GDExtensionProductionAccessV1;

/* Checked Core production physical transport. Availability is not original Runtime logical source, operation or window authority. */
typedef enum {
	GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1 = 1,
	GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1 = 2,
	GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1 = 3,
} GDExtensionProductionResourceOwnershipV1;

/* Checked Core production physical transport. Availability is not original Runtime logical source, operation or window authority. */
typedef enum {
	GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1 = 0,
	GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1 = 1,
} GDExtensionProductionListBoundaryV1;

/* Checked Core production physical transport. Availability is not original Runtime logical source, operation or window authority. */
typedef struct {
	uint64_t canonical_suboperation;
	uint32_t action_kind;
	uint32_t physical_unit;
	uint32_t first_use;
	uint32_t use_count;
	uint32_t list_boundary;
	uint32_t readback_slot;
} GDExtensionProductionActionV1;

/* Checked Core production physical transport. Availability is not original Runtime logical source, operation or window authority. */
typedef struct {
	uint32_t resource_slot;
	uint32_t access;
	uint64_t byte_offset;
	uint64_t byte_size;
	uint64_t required_usage;
} GDExtensionProductionUseV1;

/* Checked Core production physical transport. Availability is not original Runtime logical source, operation or window authority. */
typedef struct {
	uint32_t resource_kind;
	uint32_t original_slot;
	uint32_t original_part;
	uint32_t ownership;
	uint64_t original_generation;
	uint64_t physical_bytes;
	uint64_t required_usage;
} GDExtensionProductionResourceV1;

/* Checked Core production physical transport. Availability is not original Runtime logical source, operation or window authority. */
typedef struct {
	uint32_t resource_slot;
	uint32_t reserved;
	uint64_t canonical_producer;
	uint64_t canonical_copy;
	uint64_t byte_offset;
	uint64_t byte_size;
} GDExtensionProductionReadbackV1;

/* Checked Core production physical transport. Availability is not original Runtime logical source, operation or window authority.
 * The abi header uses the original DeviceExit ABI version 1 and reserved zero; struct_size is sizeof(GDExtensionProductionSetBindingV1), not sizeof(GDExtensionDeviceExitAbi). Header-size fallback is not supported.
 */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint32_t set;
	uint32_t binding;
	uint32_t array_element;
	uint32_t resource_slot;
	uint64_t offset;
	uint64_t bytes;
} GDExtensionProductionSetBindingV1;

/* Checked Core production physical transport. Availability is not original Runtime logical source, operation or window authority.
 * The abi header uses the original DeviceExit ABI version 1 and reserved zero; struct_size is sizeof(GDExtensionProductionDispatchV1), not sizeof(GDExtensionDeviceExitAbi). Header-size fallback is not supported.
 */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t canonical_suboperation;
	uint32_t unit_slot;
	uint32_t first_set_slot;
	uint32_t set_count;
	uint32_t groups_x;
	uint32_t groups_y;
	uint32_t groups_z;
	uint32_t push_bytes;
	uint32_t reserved;
} GDExtensionProductionDispatchV1;

/* Checked Core production physical transport. Availability is not original Runtime logical source, operation or window authority.
 * The abi header uses the original DeviceExit ABI version 1 and reserved zero; struct_size is sizeof(GDExtensionProductionCopyV1), not sizeof(GDExtensionDeviceExitAbi). Header-size fallback is not supported.
 */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t canonical_suboperation;
	uint32_t source_slot;
	uint32_t destination_slot;
	uint64_t source_offset;
	uint64_t destination_offset;
	uint64_t bytes;
} GDExtensionProductionCopyV1;

/* Checked Core production physical transport. Availability is not original Runtime logical source, operation or window authority.
 * The abi header uses the original DeviceExit ABI version 1 and reserved zero; struct_size is sizeof(GDExtensionProductionUploadV1), not sizeof(GDExtensionDeviceExitAbi). Header-size fallback is not supported.
 */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t canonical_suboperation;
	uint32_t destination_slot;
	uint32_t reserved;
	uint64_t destination_offset;
	uint64_t bytes;
} GDExtensionProductionUploadV1;

/* Checked Core production physical transport. Availability is not original Runtime logical source, operation or window authority. */
typedef enum {
	GDEXTENSION_PRODUCTION_REJECTED_V1 = 0,
	GDEXTENSION_PRODUCTION_APPENDED_V1 = 1,
	GDEXTENSION_PRODUCTION_UNKNOWN_V1 = 2,
	/* Known physical child prefix of the same registered canonical parent, distinct from Unknown. */
	GDEXTENSION_PRODUCTION_KNOWN_PREFIX_V1 = 3,
} GDExtensionProductionAppendStateV1;

/* Checked Core production physical transport. Availability is not original Runtime logical source, operation or window authority.
 * The abi header uses the original DeviceExit ABI version 1 and reserved zero; struct_size is sizeof(GDExtensionProductionAppendV1), not sizeof(GDExtensionDeviceExitAbi). Header-size fallback is not supported.
 */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t revision;
	uint64_t canonical_suboperation;
	uint32_t state;
	uint32_t reserved;
	GDExtensionDeviceSessionResultV1 cause;
	uint64_t recorded_bytes;
	uint64_t parent_bytes;
	uint32_t first_graph_fact;
	uint32_t graph_fact_count;
} GDExtensionProductionAppendV1;

/* Checked Core production physical transport. Availability is not original Runtime logical source, operation or window authority. */
typedef struct {
	uint64_t canonical_suboperation;
	uint64_t graph_instance;
	uint64_t graph_frame;
	uint32_t graph_kind;
	uint32_t graph_index;
	uint32_t graph_offset;
	uint32_t command_slot;
	uint32_t backend_queue;
	uint32_t issue;
	uint32_t completion;
	uint32_t phase;
	uint64_t native_serial;
	uint64_t completion_serial;
	int64_t issue_native_code;
	int64_t completion_native_code;
	int32_t issue_error;
	int32_t retention_error;
	uint32_t membership_parent;
	uint32_t reserved;
} GDExtensionProductionFactV1;

/* Checked Core production physical transport. Availability is not original Runtime logical source, operation or window authority.
 * The abi header uses the original DeviceExit ABI version 1 and reserved zero; struct_size is sizeof(GDExtensionProductionReportV1), not sizeof(GDExtensionDeviceExitAbi). Header-size fallback is not supported.
 * release_v1 uses service_control_serial and service_pending to identify the original fixed cleanup cell and its full-slice escrow. A pending slice is not reusable; a returned old unused balance is diagnostic and must not credit a new OwnerServiceMeter or change deadline.
 */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionProductionOperationV1 operation;
	uint64_t revision;
	uint64_t captured_segment;
	uint64_t acknowledged_segment;
	uint64_t recorded_end;
	uint32_t fact_count;
	uint32_t known_obligations;
	uint32_t possible_obligations;
	uint32_t complete_obligations;
	uint32_t ready_readbacks;
	uint32_t physical_returns_pending;
	uint32_t cancelled;
	uint32_t reserved;
	GDExtensionDeviceSessionResultV1 cause;
	uint64_t service_control_serial;
	uint32_t service_pending;
} GDExtensionProductionReportV1;

/* Checked Core production physical transport. Availability is not original Runtime logical source, operation or window authority.
 * The abi header uses the original DeviceExit ABI version 1 and reserved zero; struct_size is sizeof(GDExtensionProductionSegmentV1), not sizeof(GDExtensionDeviceExitAbi). Header-size fallback is not supported.
 */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t segment;
	uint64_t segment_revision;
	uint64_t first_canonical;
	uint64_t end_canonical;
	uint64_t incomplete_upload_parent;
} GDExtensionProductionSegmentV1;

/* Checked Core production physical transport. Availability is not original Runtime logical source, operation or window authority.
 * The abi header uses the original DeviceExit ABI version 1 and reserved zero; struct_size is sizeof(GDExtensionProductionServiceBudgetV1), not sizeof(GDExtensionDeviceExitAbi). Header-size fallback is not supported.
 */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t deadline_micros;
	uint32_t controls;
	uint32_t receipts;
	uint32_t passes;
	uint32_t reserved;
} GDExtensionProductionServiceBudgetV1;

/* Checked Core production physical port. Original B/Runtime logical source, operation and window authority remain separate.
 * Core issues this opaque exact protected RT invocation identity and expires it at actual callback return. No caller phase number is accepted.
 */
typedef struct {
	uint64_t registry_instance;
	uint64_t frame_serial;
} GDExtensionProductionPhysicalContextRefV1;

/* Checked Core production physical port. Original B/Runtime logical source, operation and window authority remain separate.
 * The operation is an immutable synchronous borrow; owner retains original references. The receiver must not move owner controllers onto RT.
 */
typedef void (*GDExtensionProductionRecordReceiverV1)(void *userdata, GDExtensionProductionPhysicalContextRefV1 context, GDExtensionProductionRequestRefV1 request, const GDExtensionProductionOperationV1 *operation);
/* Checked Core production physical port. Original B/Runtime logical source, operation and window authority remain separate.
 * Fresh Core owner accepted-metadata entry; no physical record context is delivered. Existing authenticated ACK/returns survive new-work seal.
 */
typedef void (*GDExtensionProductionMetadataReceiverV1)(void *userdata, GDExtensionProductionAttachmentRefV1 attachment);
typedef enum {
	GDEXTENSION_PRODUCTION_RECORD_PAUSE_NONE_V1 = 0,
	GDEXTENSION_PRODUCTION_RECORD_PAUSE_SCOPE_RETURN_V1 = 1,
	GDEXTENSION_PRODUCTION_RECORD_PAUSE_APPEND_LIMIT_V1 = 2,
	GDEXTENSION_PRODUCTION_RECORD_PAUSE_UPLOAD_PREFIX_V1 = 3,
} GDExtensionProductionRecordPauseV1;

typedef enum {
	GDEXTENSION_PRODUCTION_RECORD_HAS_APPEND_V1 = 1,
	GDEXTENSION_PRODUCTION_RECORD_HAS_SEGMENT_V1 = 2,
	GDEXTENSION_PRODUCTION_RECORD_HAS_REPORT_V1 = 4,
} GDExtensionProductionRecordReportFlagsV1;

/* Checked Core production physical port. Original B/Runtime logical source, operation and window authority remain separate.
 * One immutable fixed RECORD turn result. flags bit0 means append is present; bit1 means the latest actual sealed segment is present; bit2 means original request report is present. An older acknowledged sealed segment is not a claim that a new uncertain suffix sealed. pause 0 none, 1 scope return, 2 actual append limit, 3 known upload prefix.
 * The abi header uses original DeviceExit version 1, reserved zero, struct_size sizeof(GDExtensionProductionRecordReportV1).
 */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t request_serial;
	uint64_t control_serial;
	uint32_t pause;
	uint32_t flags;
	GDExtensionDeviceSessionResultV1 result;
	GDExtensionProductionReportV1 report;
	GDExtensionProductionAppendV1 append;
	GDExtensionProductionSegmentV1 segment;
} GDExtensionProductionRecordReportV1;

typedef enum {
	GDEXTENSION_QUALIFICATION_TEXTURE_USAGE_SAMPLING_V1 = 1,
	GDEXTENSION_QUALIFICATION_TEXTURE_USAGE_STORAGE_V1 = 2,
	GDEXTENSION_QUALIFICATION_TEXTURE_USAGE_DEPTH_ATTACHMENT_V1 = 4,
} GDExtensionQualificationTextureUsageV1;

typedef enum {
	GDEXTENSION_QUALIFICATION_RASTER_PROFILE_INVALID_V1 = 0,
	GDEXTENSION_QUALIFICATION_RASTER_PROFILE_PROCEDURAL_DEPTH_V1 = 1,
} GDExtensionQualificationRasterProfileV1;

typedef enum {
	GDEXTENSION_QUALIFICATION_RASTER_SHADER_BIT_V1 = 1,
	GDEXTENSION_QUALIFICATION_RASTER_PIPELINE_BIT_V1 = 2,
	GDEXTENSION_QUALIFICATION_RASTER_FRAMEBUFFER_BIT_V1 = 4,
} GDExtensionQualificationRasterPhysicalBitsV1;

typedef enum {
	GDEXTENSION_QUALIFICATION_RT_RASTER_CREATE_V1 = 18,
	GDEXTENSION_QUALIFICATION_RT_RASTER_RETIRE_V1 = 19,
	GDEXTENSION_QUALIFICATION_RT_RECORD_PREPARED_V4 = 20,
} GDExtensionQualificationRtDepthKindV1;

typedef struct {
	uint64_t registry_instance;
	uint64_t handle_serial;
} GDExtensionQualificationRasterRefV1;

typedef struct {
	uint64_t registry_instance;
	uint64_t handle_serial;
} GDExtensionQualificationRasterRetireRefV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionQualificationSourceV1 source;
	GDExtensionNativeUnitCreateRequestV1 unit;
	GDExtensionQualificationResourceRefV1 target;
	uint64_t target_object_serial;
	uint32_t profile;
	uint32_t push_constant_bytes;
} GDExtensionQualificationRasterCreateV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionQualificationRasterRefV1 raster;
	GDExtensionQualificationSourceV1 source;
	GDExtensionQualificationResourceRefV1 target;
	uint64_t target_object_serial;
	uint64_t shader_rid;
	uint64_t pipeline_rid;
	uint64_t framebuffer_rid;
	uint64_t shader_serial;
	uint64_t pipeline_serial;
	uint64_t framebuffer_serial;
	uint32_t known_mask;
	uint32_t possible_mask;
	uint32_t state;
	uint32_t effective_flags;
	int32_t native_error;
	uint32_t phase;
	uint64_t event_seq;
} GDExtensionQualificationRasterBackingV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionQualificationRasterRefV1 raster;
	uint64_t retire_operation;
	uint64_t reserved;
} GDExtensionQualificationRasterRetireRequestV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionQualificationRasterRefV1 raster;
	uint32_t state;
	uint32_t return_status;
	uint32_t enqueued_mask;
	uint32_t freed_mask;
	uint32_t absent_mask;
	uint32_t unresolved_mask;
	uint32_t shader_frame_slot;
	uint32_t pipeline_frame_slot;
	uint32_t framebuffer_frame_slot;
	uint64_t shader_rid;
	uint64_t pipeline_rid;
	uint64_t framebuffer_rid;
	uint64_t shader_serial;
	uint64_t pipeline_serial;
	uint64_t framebuffer_serial;
	uint64_t shader_frame_epoch;
	uint64_t pipeline_frame_epoch;
	uint64_t framebuffer_frame_epoch;
	uint64_t shader_free_event_seq;
	uint64_t pipeline_free_event_seq;
	uint64_t framebuffer_free_event_seq;
	uint64_t dependency_set_serial;
	uint64_t report_revision;
	int32_t native_error;
	uint32_t reason;
	int64_t native_code;
	uint64_t retire_operation;
} GDExtensionQualificationRasterRetireReportV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionQualificationSourceV1 source;
	GDExtensionQualificationRasterRefV1 raster;
	GDExtensionQualificationResourceRefV1 target;
	uint64_t target_object_serial;
	uint64_t expected_content_revision;
} GDExtensionQualificationDepthPassV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint32_t scissor_x;
	uint32_t scissor_y;
	uint32_t scissor_width;
	uint32_t scissor_height;
	uint32_t procedural_vertices;
	uint32_t instances;
	uint64_t push_offset;
	uint32_t push_size;
	uint32_t reserved;
} GDExtensionQualificationDepthDrawV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionQualificationRecordV3 record;
	GDExtensionQualificationDepthPassV1 depth;
} GDExtensionQualificationRecordV4;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionQualificationSourceV1 source;
	GDExtensionQualificationRasterRefV1 raster;
	GDExtensionQualificationResourceRefV1 target;
	uint64_t target_object_serial;
	uint64_t content_revision;
	uint64_t report_revision;
	uint32_t state;
	int32_t native_error;
	uint32_t phase;
	uint32_t list_state;
	uint32_t begin_fact;
	uint32_t clear_fact;
	uint32_t end_fact;
	uint32_t cleanup_end_fact;
	uint32_t first_draw_fact;
	uint32_t known_draw_count;
	uint32_t possible_draw_count;
	uint32_t required_draw_count;
} GDExtensionQualificationDepthReportV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionQualificationReportV2 record;
	GDExtensionQualificationDepthReportV1 depth;
} GDExtensionQualificationReportV3;

/* Checked Core V2 physical action-ordinal transport. Original C04/C06/R01 source mapping, logical requirement, once/window and publication receipt authority remain with their original owners.
 * This row has no abi header; its layout is selected only by the typed V2 entry point. Never pass it through a V1 row-array interface.
 * action_ordinal identifies the physical array row; canonical_suboperation and source_recipe_substep are retained correlation from that frozen row, never source coverage or recording authority. D does not infer recipe cardinality or require increasing substeps.
 * action_ordinal equals the array index. Original canonical rows are in the unchanged nonempty logical operation range and nondecreasing; repeated canonicals represent multiple physical actions and missing canonicals may have zero physical actions.
 */
typedef struct {
	/* Physical action array index. It must identify the same frozen row as the canonical and source recipe substep. */
	uint64_t action_ordinal;
	uint64_t canonical_suboperation;
	/* Original source recipe substep correlation. Retained and cross-checked, never issued or inferred by Core. */
	uint32_t source_recipe_substep;
	/* Must be zero. */
	uint32_t source_recipe_reserved;
	uint32_t action_kind;
	uint32_t physical_unit;
	uint32_t first_use;
	uint32_t use_count;
	uint32_t list_boundary;
	uint32_t readback_slot;
} GDExtensionProductionActionV2;

/* Checked Core V2 physical action-ordinal transport. Original C04/C06/R01 source mapping, logical requirement, once/window and publication receipt authority remain with their original owners.
 * The abi header uses abi_version 2, struct_size equal to sizeof this exact V2 DTO, and reserved zero. Reused OperationV1, CapacityV1 and ServiceBudgetV1 keep their original version 1 headers. No V1 DTO is reinterpreted.
 * action_ordinal identifies the physical array row; canonical_suboperation and source_recipe_substep are retained correlation from that frozen row, never source coverage or recording authority. D does not infer recipe cardinality or require increasing substeps.
 */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	/* Physical action array index. It must identify the same frozen row as the canonical and source recipe substep. */
	uint64_t action_ordinal;
	uint64_t canonical_suboperation;
	/* Original source recipe substep correlation. Retained and cross-checked, never issued or inferred by Core. */
	uint32_t source_recipe_substep;
	/* Must be zero. */
	uint32_t source_recipe_reserved;
	uint32_t unit_slot;
	uint32_t first_set_slot;
	uint32_t set_count;
	uint32_t groups_x;
	uint32_t groups_y;
	uint32_t groups_z;
	uint32_t push_bytes;
	uint32_t reserved;
} GDExtensionProductionDispatchV2;

/* Checked Core V2 physical action-ordinal transport. Original C04/C06/R01 source mapping, logical requirement, once/window and publication receipt authority remain with their original owners.
 * The abi header uses abi_version 2, struct_size equal to sizeof this exact V2 DTO, and reserved zero. Reused OperationV1, CapacityV1 and ServiceBudgetV1 keep their original version 1 headers. No V1 DTO is reinterpreted.
 * action_ordinal identifies the physical array row; canonical_suboperation and source_recipe_substep are retained correlation from that frozen row, never source coverage or recording authority. D does not infer recipe cardinality or require increasing substeps.
 */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	/* Physical action array index. It must identify the same frozen row as the canonical and source recipe substep. */
	uint64_t action_ordinal;
	uint64_t canonical_suboperation;
	/* Original source recipe substep correlation. Retained and cross-checked, never issued or inferred by Core. */
	uint32_t source_recipe_substep;
	/* Must be zero. */
	uint32_t source_recipe_reserved;
	uint32_t source_slot;
	uint32_t destination_slot;
	uint64_t source_offset;
	uint64_t destination_offset;
	uint64_t bytes;
} GDExtensionProductionCopyV2;

/* Checked Core V2 physical action-ordinal transport. Original C04/C06/R01 source mapping, logical requirement, once/window and publication receipt authority remain with their original owners.
 * The abi header uses abi_version 2, struct_size equal to sizeof this exact V2 DTO, and reserved zero. Reused OperationV1, CapacityV1 and ServiceBudgetV1 keep their original version 1 headers. No V1 DTO is reinterpreted.
 * action_ordinal identifies the physical array row; canonical_suboperation and source_recipe_substep are retained correlation from that frozen row, never source coverage or recording authority. D does not infer recipe cardinality or require increasing substeps.
 */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	/* Physical action array index. It must identify the same frozen row as the canonical and source recipe substep. */
	uint64_t action_ordinal;
	uint64_t canonical_suboperation;
	/* Original source recipe substep correlation. Retained and cross-checked, never issued or inferred by Core. */
	uint32_t source_recipe_substep;
	/* Must be zero. */
	uint32_t source_recipe_reserved;
	uint32_t destination_slot;
	uint32_t reserved;
	uint64_t destination_offset;
	uint64_t bytes;
} GDExtensionProductionUploadV2;

/* Checked Core V2 physical action-ordinal transport. Original C04/C06/R01 source mapping, logical requirement, once/window and publication receipt authority remain with their original owners.
 * This row has no abi header; its layout is selected only by the typed V2 entry point. Never pass it through a V1 row-array interface.
 * The resource, byte offset and byte size retain their original physical meaning. Original logical canonical provenance is obtained from the frozen endpoint rows, not from an invented physical canonical range.
 */
typedef struct {
	uint32_t resource_slot;
	uint32_t reserved;
	/* Physical action ordinal in this same frozen request. Producer must precede the unique READBACK copy row; both rows may have the same original canonical. */
	uint64_t producer_action_ordinal;
	/* Physical action ordinal in this same frozen request. Producer must precede the unique READBACK copy row; both rows may have the same original canonical. */
	uint64_t copy_action_ordinal;
	uint64_t byte_offset;
	uint64_t byte_size;
} GDExtensionProductionReadbackV2;

/* Explicit physical readback source kind. The row tag is uint32_t; enum ABI size is not used for wire layout. */
typedef enum {
	GDEXTENSION_PRODUCTION_READBACK_REQUEST_WRITER_V1 = 1,
	GDEXTENSION_PRODUCTION_READBACK_RETAINED_INCOMING_V1 = 2,
	GDEXTENSION_PRODUCTION_READBACK_NATIVE_OUTPUT_GUARD_V1 = 3,
} GDExtensionProductionReadbackSourceKindV1;

typedef struct {
	/* Real same-request writer ordinal; zero is a valid writer ordinal, never a no-producer sentinel. */
	uint64_t producer_action_ordinal;
} GDExtensionProductionReadbackWriterSourceV1;

typedef struct {
	/* Nonzero original retained storage generation, cross-checked against the imported resource. */
	uint64_t original_generation;
	/* Nonzero original physical backing serial, cross-checked against the authenticated import and live RD object. */
	uint64_t backing_serial;
} GDExtensionProductionReadbackIncomingSourceV1;

/* Explicit 56-byte physical readback source row, selected only by device_session_production_reserve_readback_sources_v1. Inactive payload fields must be zero; source_kind selects interpretation, never an ordinal sentinel.
 * REQUEST_WRITER preserves the actual V2 same-request writer and full write-coverage requirements. RETAINED_INCOMING has no producer action and permits copy ordinal zero; only an original RETAINED_PREPARED STORAGE import is eligible.
 * Incoming protects the exact physical range from earlier writes in this request, including aliases and full reflected writable binding windows. An earlier NATIVE_OUTPUT or unresolved write target is unsupported. Original E05/R01 lease, stable content version, complete source and ordered dependency authority remain with their owners.
 * Opaque physical copying does not certify initialized padding/inactive bytes or grant semantic decoding/raw export authority. No content initialization pass is implied.
 * NATIVE_OUTPUT_GUARD selects the exact K-owned GuardStatus32 from the same request original output Finalize named by writer.producer_action_ordinal. Incoming fields are zero, offset0/size32, OUTPUT_ALIAS source with exact READ-only uses exclusively on its declared READBACK row. No writable output role, caller RID, shader access, incoming import or completion claim is granted.
 */
typedef struct {
	uint32_t resource_slot;
	uint32_t source_kind;
	uint64_t copy_action_ordinal;
	uint64_t byte_offset;
	uint64_t byte_size;
	GDExtensionProductionReadbackWriterSourceV1 writer;
	GDExtensionProductionReadbackIncomingSourceV1 incoming;
} GDExtensionProductionReadbackSourceV1;

/* Checked Core V2 physical action-ordinal transport. Original C04/C06/R01 source mapping, logical requirement, once/window and publication receipt authority remain with their original owners.
 * The abi header uses abi_version 2, struct_size equal to sizeof this exact V2 DTO, and reserved zero. Reused OperationV1, CapacityV1 and ServiceBudgetV1 keep their original version 1 headers. No V1 DTO is reinterpreted.
 * action_ordinal identifies the physical array row; canonical_suboperation and source_recipe_substep are retained correlation from that frozen row, never source coverage or recording authority. D does not infer recipe cardinality or require increasing substeps.
 */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t revision;
	/* Physical action array index. It must identify the same frozen row as the canonical and source recipe substep. */
	uint64_t action_ordinal;
	uint64_t canonical_suboperation;
	/* Original source recipe substep correlation. Retained and cross-checked, never issued or inferred by Core. */
	uint32_t source_recipe_substep;
	/* Must be zero. */
	uint32_t source_recipe_reserved;
	uint32_t state;
	uint32_t reserved;
	GDExtensionDeviceSessionResultV1 cause;
	uint64_t recorded_bytes;
	uint64_t parent_bytes;
	uint32_t first_graph_fact;
	uint32_t graph_fact_count;
} GDExtensionProductionAppendV2;

/* Checked Core V2 physical action-ordinal transport. Original C04/C06/R01 source mapping, logical requirement, once/window and publication receipt authority remain with their original owners.
 * This row has no abi header; its layout is selected only by the typed V2 entry point. Never pass it through a V1 row-array interface.
 * action_ordinal identifies the physical array row; canonical_suboperation and source_recipe_substep are retained correlation from that frozen row, never source coverage or recording authority. D does not infer recipe cardinality or require increasing substeps.
 * membership_parent remains a graph-fact index, not an action ordinal. Native aliases retain their original graph membership and physical source identity.
 */
typedef struct {
	/* Physical action array index. It must identify the same frozen row as the canonical and source recipe substep. */
	uint64_t action_ordinal;
	uint64_t canonical_suboperation;
	/* Original source recipe substep correlation. Retained and cross-checked, never issued or inferred by Core. */
	uint32_t source_recipe_substep;
	/* Must be zero. */
	uint32_t source_recipe_reserved;
	uint64_t graph_instance;
	uint64_t graph_frame;
	uint32_t graph_kind;
	uint32_t graph_index;
	uint32_t graph_offset;
	uint32_t command_slot;
	uint32_t backend_queue;
	uint32_t issue;
	uint32_t completion;
	uint32_t phase;
	uint64_t native_serial;
	uint64_t completion_serial;
	int64_t issue_native_code;
	int64_t completion_native_code;
	int32_t issue_error;
	int32_t retention_error;
	uint32_t membership_parent;
	uint32_t reserved;
} GDExtensionProductionFactV2;

/* Checked Core V2 physical action-ordinal transport. Original C04/C06/R01 source mapping, logical requirement, once/window and publication receipt authority remain with their original owners.
 * The abi header uses abi_version 2, struct_size equal to sizeof this exact V2 DTO, and reserved zero. Reused OperationV1, CapacityV1 and ServiceBudgetV1 keep their original version 1 headers. No V1 DTO is reinterpreted.
 * OperationV1 retains the original logical canonical_begin/end, owner nonce and record generation. Physical exhaustion, including zero rows, never proves logical coverage or publication receipts. Existing cleanup service escrow fields keep their original meaning.
 */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionProductionOperationV1 operation;
	uint64_t revision;
	uint64_t captured_segment;
	uint64_t acknowledged_segment;
	/* Exclusive physical action frontier. This is not a logical canonical completion frontier. */
	uint64_t recorded_action_end;
	/* Total frozen physical action rows, independent of the original logical operation range. */
	uint64_t action_count;
	uint32_t fact_count;
	uint32_t known_obligations;
	uint32_t possible_obligations;
	uint32_t complete_obligations;
	uint32_t ready_readbacks;
	uint32_t physical_returns_pending;
	uint32_t cancelled;
	uint32_t reserved;
	GDExtensionDeviceSessionResultV1 cause;
	uint64_t service_control_serial;
	uint32_t service_pending;
} GDExtensionProductionReportV2;

/* Checked Core V2 physical action-ordinal transport. Original C04/C06/R01 source mapping, logical requirement, once/window and publication receipt authority remain with their original owners.
 * The abi header uses abi_version 2, struct_size equal to sizeof this exact V2 DTO, and reserved zero. Reused OperationV1, CapacityV1 and ServiceBudgetV1 keep their original version 1 headers. No V1 DTO is reinterpreted.
 */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t segment;
	uint64_t segment_revision;
	/* Physical action endpoint of this original immutable segment. */
	uint64_t first_action_ordinal;
	/* Physical action endpoint of this original immutable segment. */
	uint64_t end_action_ordinal;
	/* Exact physical upload parent ordinal for a known child prefix. UINT64_MAX means none. */
	uint64_t incomplete_upload_action_ordinal;
} GDExtensionProductionSegmentV2;

/* Checked Core V2 physical action-ordinal transport. Original C04/C06/R01 source mapping, logical requirement, once/window and publication receipt authority remain with their original owners.
 * The abi header uses abi_version 2, struct_size equal to sizeof this exact V2 DTO, and reserved zero. Reused OperationV1, CapacityV1 and ServiceBudgetV1 keep their original version 1 headers. No V1 DTO is reinterpreted.
 * The immutable original FIFO return retains exact request/control identity and V2 nested physical evidence. A V1 consumer must not consume this V2 return, or vice versa.
 */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t request_serial;
	uint64_t control_serial;
	uint32_t pause;
	uint32_t flags;
	GDExtensionDeviceSessionResultV1 result;
	GDExtensionProductionReportV2 report;
	GDExtensionProductionAppendV2 append;
	GDExtensionProductionSegmentV2 segment;
} GDExtensionProductionRecordReportV2;

typedef struct {
	uint8_t bytes[32];
} GDExtensionOutputTokenV1;

typedef struct {
	double position[3];
	double size[3];
} GDExtensionOutputBoundsV1;

typedef struct {
	uint64_t view_id;
	uint64_t view_epoch;
	uint64_t capture_ordinal;
} GDExtensionOutputCameraV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionOutputCameraV1 camera;
	uint64_t present_seq;
	float eye[3];
	float forward[3];
	float right[3];
	float up[3];
} GDExtensionProductionCameraSampleV1;

typedef struct {
	const uint64_t *data;
	uint32_t count;
	uint32_t reserved;
} GDExtensionOutputRidSpanV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint32_t profile;
	uint32_t capacity;
	uint32_t max_views;
	uint32_t max_inflight;
	uint32_t space;
	uint32_t pass_mask;
	uint32_t motion;
	uint32_t query_enabled;
	GDExtensionOutputRidSpanV1 surfaces;
	GDExtensionOutputRidSpanV1 surface_materials;
	GDExtensionOutputBoundsV1 bounds;
	GDExtensionOutputBoundsV1 geometry_bounds;
	GDExtensionOutputBoundsV1 query_bounds;
	GDExtensionOutputTokenV1 producer_tag;
	GDExtensionOutputTokenV1 device_tag;
	GDExtensionOutputTokenV1 layout_digest;
	GDExtensionOutputTokenV1 mapping_digest;
	GDExtensionOutputTokenV1 numeric_profile_digest;
} GDExtensionOutputCreateInputV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionOutputCreateInputV1 config;
	uint32_t metadata_resource_slot;
	uint32_t reserved;
} GDExtensionOutputCreateInputV2;

typedef struct {
	uint64_t generation;
	uint64_t present_seq;
	GDExtensionOutputTokenV1 output_tag;
	GDExtensionOutputTokenV1 view_tag;
	GDExtensionOutputBoundsV1 bounds;
	uint32_t history_reset;
	uint32_t has_actual_previous_camera;
	uint32_t has_previous_consumed_tuple;
	uint32_t reserved;
	GDExtensionOutputCameraV1 current_camera;
	GDExtensionOutputCameraV1 actual_previous_camera;
	GDExtensionOutputTokenV1 current_host_token;
	GDExtensionOutputTokenV1 previous_consumed_tuple_token;
} GDExtensionOutputCommonInputV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t particles;
	GDExtensionOutputCommonInputV1 common;
	uint64_t content_version;
	uint64_t view_epoch;
	uint32_t pass_mask;
	uint32_t reserved;
	GDExtensionOutputTokenV1 layout_digest;
	GDExtensionOutputTokenV1 mapping_digest;
	GDExtensionOutputTokenV1 source_token;
} GDExtensionOutputAcquireInputV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t particles;
	GDExtensionOutputTokenV1 lease_token;
	GDExtensionOutputCommonInputV1 common;
	GDExtensionOutputTokenV1 record_token;
} GDExtensionOutputSealInputV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t particles;
	GDExtensionOutputTokenV1 lease_token;
} GDExtensionOutputCancelInputV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t particles;
	uint64_t generation;
} GDExtensionOutputGenerationInputV1;

typedef struct {
	int32_t error;
	uint32_t detail_length;
	const char *detail;
} GDExtensionOutputStatusV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionOutputStatusV1 status;
	uint64_t particles;
	uint64_t generation;
	uint32_t capabilities;
	uint32_t reserved;
} GDExtensionOutputCreateResultV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint32_t view_lease_capacity;
	uint32_t read_lease_capacity;
	uint32_t surface_lease_capacity;
	uint32_t reserved;
} GDExtensionOutputMetadataLimitsV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t required_metadata_bytes;
	uint32_t layout_version;
	uint32_t required_alignment;
	uint32_t slot_count;
	uint32_t flattened_surface_count;
	uint32_t binding_count;
	uint32_t reserved;
	GDExtensionOutputMetadataLimitsV1 limits;
} GDExtensionOutputMetadataRequirementsV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionOutputCreateInputV1 config;
	GDExtensionOutputMetadataLimitsV1 limits;
} GDExtensionOutputMetadataQueryInputV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionOutputStatusV1 status;
	GDExtensionOutputMetadataRequirementsV1 requirements;
} GDExtensionOutputMetadataQueryResultV1;

typedef enum {
	GDEXTENSION_QUALIFICATION_RT_OUTPUT_METADATA_QUERY_V1 = 24,
} GDExtensionQualificationRtOutputKindV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	int32_t error;
	uint32_t detail_length;
	char detail[96];
	GDExtensionOutputMetadataRequirementsV1 requirements;
} GDExtensionOutputMetadataQueryRtResultV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionOutputCreateInputV1 config;
	GDExtensionOutputMetadataRequirementsV1 requirements;
	uint32_t metadata_resource_slot;
	uint32_t reserved;
} GDExtensionOutputCreateInputV3;

typedef struct {
	uint32_t role;
	uint32_t access;
	uint64_t rid;
	uint64_t allocation_generation;
	uint32_t byte_length;
	uint32_t stride;
} GDExtensionOutputWritableV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionOutputStatusV1 status;
	uint32_t state;
	uint32_t reserved;
	GDExtensionOutputTokenV1 packet_token;
} GDExtensionOutputSealResultV1;

/* Fixed symbolic constants. All corresponding DTO fields remain uint32_t. */
typedef enum {
	GDEXTENSION_OUTPUT_ROLE_INVALID_V1 = 0,
	GDEXTENSION_OUTPUT_ROLE_CURRENT_RECORDS_V1 = 1,
	GDEXTENSION_OUTPUT_ROLE_ALIGNED_PREVIOUS_RECORDS_V1 = 2,
	GDEXTENSION_OUTPUT_ROLE_CURRENT_VERTICES_V1 = 3,
	GDEXTENSION_OUTPUT_ROLE_ALIGNED_PREVIOUS_VERTICES_V1 = 4,
	GDEXTENSION_OUTPUT_ROLE_VERTEX_HISTORY_VALIDITY_V1 = 5,
	GDEXTENSION_OUTPUT_ROLE_INDEX_STAGING_V1 = 6,
	GDEXTENSION_OUTPUT_ROLE_OUTPUT_META_V1 = 7,
	GDEXTENSION_OUTPUT_ACCESS_INVALID_V1 = 0,
	GDEXTENSION_OUTPUT_ACCESS_WRITE_ONLY_V1 = 1,
	GDEXTENSION_OUTPUT_SEAL_INVALID_V1 = 0,
	GDEXTENSION_OUTPUT_SEAL_RECORDED_ORDERED_V1 = 1,
} GDExtensionOutputConstantsV1;

typedef enum {
	GDEXTENSION_PRODUCTION_RECORD_HISTORY_SCHEDULE_ATTEMPTED_V1 = 1,
	GDEXTENSION_PRODUCTION_RECORD_HISTORY_QUEUE_POSSIBLE_V1 = 2,
	GDEXTENSION_PRODUCTION_RECORD_HISTORY_ADMITTED_V1 = 4,
	GDEXTENSION_PRODUCTION_RECORD_HISTORY_SCHEDULE_RESULT_VALID_V1 = 8,
	GDEXTENSION_PRODUCTION_RECORD_HISTORY_CONTROL_EFFECT_ENTERED_V1 = 16,
	GDEXTENSION_PRODUCTION_RECORD_HISTORY_REQUEST_EFFECT_ENTERED_V1 = 32,
	GDEXTENSION_PRODUCTION_RECORD_HISTORY_RETURNED_V1 = 64,
	GDEXTENSION_PRODUCTION_RECORD_HISTORY_OWNER_CONSUMED_V1 = 128,
	GDEXTENSION_PRODUCTION_RECORD_HISTORY_TERMINAL_NO_EFFECT_V1 = 256,
	GDEXTENSION_PRODUCTION_RECORD_HISTORY_CONTROL_APPEND_ENTERED_V1 = 512,
	GDEXTENSION_PRODUCTION_RECORD_HISTORY_REQUEST_APPEND_ENTERED_V1 = 1024,
	GDEXTENSION_PRODUCTION_RECORD_HISTORY_TERMINAL_NO_APPEND_V1 = 2048,
} GDExtensionProductionRecordHistoryFlagsV1;

typedef enum {
	GDEXTENSION_PRODUCTION_RECORD_HISTORY_PENDING_V1 = 1,
	GDEXTENSION_PRODUCTION_RECORD_HISTORY_QUEUED_V1 = 2,
	GDEXTENSION_PRODUCTION_RECORD_HISTORY_EXECUTING_V1 = 3,
	GDEXTENSION_PRODUCTION_RECORD_HISTORY_RETURNED_STATE_V1 = 4,
	GDEXTENSION_PRODUCTION_RECORD_HISTORY_OWNER_CONSUMED_STATE_V1 = 5,
} GDExtensionProductionRecordHistoryStateV1;

typedef enum {
	GDEXTENSION_PRODUCTION_CONTROL_RECEIPT_NONE_V1 = 0,
	GDEXTENSION_PRODUCTION_CONTROL_RECEIPT_PENDING_V1 = 1,
	GDEXTENSION_PRODUCTION_CONTROL_RECEIPT_QUEUED_V1 = 2,
	GDEXTENSION_PRODUCTION_CONTROL_RECEIPT_EXECUTING_V1 = 3,
	GDEXTENSION_PRODUCTION_CONTROL_RECEIPT_RETURNED_V1 = 4,
	GDEXTENSION_PRODUCTION_CONTROL_RECEIPT_OWNER_CONSUMED_V1 = 5,
} GDExtensionProductionControlReceiptStateV1;

typedef enum {
	GDEXTENSION_PRODUCTION_CONTROL_RECEIPT_KIND_NONE_V1 = 0,
	GDEXTENSION_PRODUCTION_CONTROL_RECEIPT_KIND_OBSERVE_V1 = 1,
	GDEXTENSION_PRODUCTION_CONTROL_RECEIPT_KIND_CANCEL_V1 = 2,
	GDEXTENSION_PRODUCTION_CONTROL_RECEIPT_KIND_RELEASE_V1 = 3,
	GDEXTENSION_PRODUCTION_CONTROL_RECEIPT_KIND_STORAGE_CREATE_V1 = 4,
	GDEXTENSION_PRODUCTION_CONTROL_RECEIPT_KIND_SET_CREATE_V1 = 5,
	GDEXTENSION_PRODUCTION_CONTROL_RECEIPT_KIND_RASTER_CREATE_V1 = 6,
	GDEXTENSION_PRODUCTION_CONTROL_RECEIPT_KIND_RECORD_V1 = 7,
} GDExtensionProductionControlReceiptKindV1;

/* Read-only projection of the original fixed request control cell. State NONE means no control was accepted. PENDING is accepted but unpublished; QUEUED or EXECUTING remains outstanding; RETURNED still requires the original consumer; OWNER_CONSUMED has returned control custody even if its result is Busy or Unavailable. Results are valid only when their explicit validity member is one. Retained until the next accepted control, not an archive, cancellation, execution grant or proof of physical retirement. */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t registry_instance;
	uint64_t request_serial;
	uint64_t control_serial;
	uint32_t kind;
	uint32_t state;
	uint32_t schedule_result_valid;
	uint32_t result_valid;
	GDExtensionDeviceSessionResultV1 last_schedule_result;
	GDExtensionDeviceSessionResultV1 control_result;
} GDExtensionProductionControlReceiptV1;

/* Original exact request/control history. Native effect flags include resource preparation and cleanup and do not prove append entry. Append flags separately track original append-capable attempts. TERMINAL_NO_APPEND may coexist with native effects that still require physical drain/release. Neither terminal flag authorizes resetting Runtime AppendStarted or replaying its once. Consuming keeps this history until the next accepted control. */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t request_serial;
	uint64_t control_serial;
	uint64_t record_generation;
	uint32_t state;
	uint32_t flags;
	GDExtensionDeviceSessionResultV1 last_schedule_result;
	GDExtensionDeviceSessionResultV1 record_result;
} GDExtensionProductionRecordHistoryV1;

/* Independent request-owned raster preparation. No qualification ownership or caller RID grants authority. Mask bits are framebuffer, depth, shader, pipeline; phases 1 depth, 2 shader, 3 framebuffer, 4 pipeline, 5 created. Native physical return remains independent. */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t create_attempt;
	uint32_t width;
	uint32_t height;
	uint64_t shader_bytes;
	uint64_t reserved;
} GDExtensionProductionRasterPrepareV1;

/* Independent request-owned raster preparation. No qualification ownership or caller RID grants authority. Mask bits are framebuffer, depth, shader, pipeline; phases 1 depth, 2 shader, 3 framebuffer, 4 pipeline, 5 created. Native physical return remains independent. */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t request_serial;
	uint64_t control_serial;
	uint64_t create_attempt;
	uint64_t revision;
	uint32_t known_mask;
	uint32_t returned_mask;
	uint32_t phase;
	uint32_t reserved;
	GDExtensionDeviceSessionResultV1 result;
	uint64_t framebuffer_serial;
	uint64_t depth_serial;
	uint64_t shader_serial;
	uint64_t pipeline_serial;
} GDExtensionProductionRasterPrepareReportV1;

/* One immutable original native-output role association. Role uses the existing OutputConstantsV1 values; protected native commands and status are never producer writable. Absent role has zero bytes and UINT32_MAX locators. */
typedef struct {
	uint32_t role;
	uint32_t resource_slot;
	uint32_t use_index;
	uint32_t reserved;
	uint64_t allocation_generation;
	uint64_t bytes;
} GDExtensionProductionOutputRoleV1;

/* Frozen native action preparation, retaining original accepted resource/use locators and exact acquire input. The ordinal is the native request action ordinal, never the compiler Output SOURCE ordinal.
 * The three role entries are current records, aligned previous records and output metadata, in that order. Physical source association does not establish original Runtime selected-root or account authority; the original caller retains and checks those owners.
 * first_source_use/source_use_count select the complete contiguous source READ-use span inside this original accepted action. Every source backing, count, attribute, identity, validity and instance gate uses its own original use row with exact resource slot, byte offset, byte size and original resource generation. All action READ storage uses must belong to this span; the single source_resource_slot/source_use_index/source_generation is only a checked anchor within it and never grants authority over other sources. Original typed mapping retains each semantic role and maps it to these accepted rows.
 */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t action_ordinal;
	uint64_t canonical_suboperation;
	uint32_t source_recipe_substep;
	uint32_t source_resource_slot;
	uint32_t source_use_index;
	uint32_t reserved;
	uint64_t source_generation;
	uint32_t first_source_use;
	uint32_t source_use_count;
	uint32_t read_capacity;
	uint32_t read_graph_capacity;
	GDExtensionProductionOutputRoleV1 roles[3];
	GDExtensionOutputAcquireInputV1 input;
} GDExtensionProductionOutputPrepareV1;

/* Read-only generation sizing and finite recipe input, available before HostView/camera capture. The exact retained original generation request authenticates the live immutable sizing snapshot. No caller capacity, surface count, camera, bounds, work or native multiplier is accepted. Native Acquire/Finalize/Seal ordinals retain their original recipe substeps. No reservation, source or runtime Acquire authority is created. */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionProductionRequestRefV1 generation_request;
	uint64_t particles;
	uint64_t generation;
	uint64_t acquire_action_ordinal;
	uint64_t acquire_canonical_suboperation;
	uint64_t finalize_action_ordinal;
	uint64_t finalize_canonical_suboperation;
	uint64_t seal_action_ordinal;
	uint64_t seal_canonical_suboperation;
	uint32_t acquire_source_recipe_substep;
	uint32_t finalize_source_recipe_substep;
	uint32_t seal_source_recipe_substep;
	uint32_t read_capacity;
	uint32_t read_graph_capacity;
	uint32_t source_space;
	uint32_t reserved;
} GDExtensionProductionOutputDemandInputV1;

/* Checked live-driver demand projection, written only with the complete quote on success. prefix/suffix exclude this guard; suffix includes all later staging and reserved native read graphs. Guard limits describe only proof upload and guard dispatches. Recheck at reserve, freeze and finalize; no generation or logical source authority is granted. */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t acquire_action_ordinal;
	uint64_t finalize_action_ordinal;
	uint64_t particles;
	uint64_t generation;
	uint64_t guard_work;
	uint64_t proof_bytes;
	uint64_t required_output_use_cells;
	uint64_t required_owned_groups;
	uint32_t capacity;
	uint32_t surfaces;
	uint32_t max_groups_x;
	uint32_t native_workers;
	uint32_t guard_dispatches;
	uint32_t proof_upload_graphs;
	uint32_t guard_graphs;
	uint32_t guard_native_memberships;
	uint32_t guard_owned_resources;
	uint32_t prefix_graphs;
	uint32_t suffix_graphs;
	uint32_t total_graphs;
	uint32_t total_native_memberships;
	uint32_t read_graphs;
	uint32_t reserved;
} GDExtensionProductionOutputGuardDemandV1;

typedef enum {
	GDEXTENSION_OUTPUT_NO_CUSTODY_V1 = 0,
	GDEXTENSION_OUTPUT_CUSTODY_ACCEPTED_V1 = 1,
	GDEXTENSION_OUTPUT_CUSTODY_RETAINED_PREFIX_V1 = 2,
	GDEXTENSION_OUTPUT_CUSTODY_UNKNOWN_V1 = 3,
} GDExtensionOutputCustodyV1;

/* Actual native acquisition observation. A failure with retained-prefix or unknown custody must keep its original source and physical retirement obligations; status alone is not a no-effect receipt.
 * Writable descriptors retain exact original resource/use locators; a returned RID cannot import authority into another request. The copied token correlates the actual retained lease; token bytes alone grant no new authority.
 * state uses OutputLeaseStateV1; INVALID conveys no validated lease observation.
 * With no validated writable descriptors, resource_slots and use_indices are UINT32_MAX. action_ordinal is UINT64_MAX until an original action association is validated; writable_count alone bounds the active descriptors.
 */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionOutputStatusV1 status;
	uint32_t custody;
	uint32_t state;
	GDExtensionOutputTokenV1 lease_token;
	uint32_t writable_count;
	uint32_t reserved;
	GDExtensionOutputWritableV1 writables[3];
	uint32_t resource_slots[3];
	uint32_t use_indices[3];
	uint64_t request_serial;
	uint64_t action_ordinal;
} GDExtensionOutputAcquireResultV1;

/* Read-only observation of the exact native output owner and original request. Does not schedule, append, advance an action or infer completion from time. known/possible/complete describe the original request obligations; physical_returned counts actual output physical returns across the whole validated generation.
 * state uses OutputGenerationStateV1; INVALID conveys no validated generation observation.
 * The request must match the original generation request or an actual slot request. action_ordinal identifies the unique matching slot action, or UINT64_MAX when no unique action association exists. Released remains observable until the actual metadata owner is freed; an empty returned generation pin does not imply that the owner is absent.
 */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionOutputStatusV1 status;
	uint64_t generation;
	uint32_t state;
	uint32_t custody;
	uint64_t known;
	uint64_t possible;
	uint64_t complete;
	uint64_t physical_returned;
	uint64_t request_serial;
	uint64_t action_ordinal;
} GDExtensionOutputPollResultV1;

/* Observable native owner state. INVALID is zero and denotes no validated owner observation, not a completion state. */
typedef enum {
	GDEXTENSION_OUTPUT_GENERATION_INVALID_V1 = 0,
	GDEXTENSION_OUTPUT_GENERATION_CONFIGURED_V1 = 1,
	GDEXTENSION_OUTPUT_GENERATION_ACTIVE_V1 = 2,
	GDEXTENSION_OUTPUT_GENERATION_WITHDRAWING_V1 = 3,
	GDEXTENSION_OUTPUT_GENERATION_RETIRING_V1 = 4,
	GDEXTENSION_OUTPUT_GENERATION_FAULTED_RETIRING_V1 = 5,
	GDEXTENSION_OUTPUT_GENERATION_RELEASED_V1 = 6,
} GDExtensionOutputGenerationStateV1;

/* Observable native owner state. INVALID is zero and denotes no validated owner observation, not a completion state. */
typedef enum {
	GDEXTENSION_OUTPUT_LEASE_INVALID_V1 = 0,
	GDEXTENSION_OUTPUT_LEASE_EMPTY_V1 = 1,
	GDEXTENSION_OUTPUT_LEASE_RESERVED_V1 = 2,
	GDEXTENSION_OUTPUT_LEASE_WRITING_V1 = 3,
	GDEXTENSION_OUTPUT_LEASE_SEALED_RECORDED_V1 = 4,
	GDEXTENSION_OUTPUT_LEASE_ADOPTED_V1 = 5,
	GDEXTENSION_OUTPUT_LEASE_EMPTY_OR_FAULT_PACKET_V1 = 6,
	GDEXTENSION_OUTPUT_LEASE_RETIRING_V1 = 7,
	GDEXTENSION_OUTPUT_LEASE_RELEASED_V1 = 8,
} GDExtensionOutputLeaseStateV1;

typedef struct {
	uint8_t bytes[32];
} GDExtensionSceneOpaque32V1;

typedef struct {
	GDExtensionSceneOpaque32V1 token;
} GDExtensionNormalRelayRefV1;

typedef struct {
	GDExtensionSceneOpaque32V1 token;
} GDExtensionScenePhaseRegistrationRefV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionSceneOpaque32V1 device_capability;
	uint32_t phase;
	uint32_t max_operations;
} GDExtensionScenePhaseSinkConfigV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionSceneOpaque32V1 device_capability;
	GDExtensionSceneOpaque32V1 phase_token;
	GDExtensionSceneOpaque32V1 physical_context;
	uint64_t occurrence;
	uint64_t expiry_serial;
} GDExtensionNativeScenePhaseV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint32_t registration_closed;
	uint32_t active_deliveries;
	uint32_t relay_cell_released;
	uint32_t relay_holder_closed;
} GDExtensionScenePhaseCloseV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint32_t entry_closed;
	uint32_t active_deliveries;
	uint32_t relay_cell_released;
	uint32_t relay_holder_closed;
} GDExtensionCameraReadyCloseV1;

/* Core camera-ready notification delivered through its original protected normal-entry holder. It does not issue U05 view or RECORD authority. */
typedef void (*GDExtensionProductionCameraReadyReceiverV1)(void *userdata, GDExtensionProductionAttachmentRefV1 attachment, const GDExtensionProductionCameraSampleV1 *sample);
typedef void (*GDExtensionScenePhaseReceiverV1)(void *userdata, const GDExtensionNativeScenePhaseV1 *phase, GDExtensionProductionRequestRefV1 request, const GDExtensionProductionOperationV1 *operation);
typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t owner_generation;
	GDExtensionProductionOperationV1 transport_operation;
	GDExtensionSceneOpaque32V1 record_request;
	GDExtensionSceneOpaque32V1 operation_ref;
	GDExtensionSceneOpaque32V1 cursor_ref;
	GDExtensionSceneOpaque32V1 schedule_digest;
} GDExtensionSceneOperationKeyV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	int32_t native_error;
	uint32_t outcome;
	uint32_t detail;
	uint32_t reserved;
	GDExtensionSceneOpaque32V1 phase_borrow;
	GDExtensionSceneOpaque32V1 record_permit;
	GDExtensionSceneOpaque32V1 record_fact;
} GDExtensionSceneReadOutcomeV1;

/* Locator to one original Scene-depth child on the retained original DeviceExit bridge; observations never reconstruct this child. */
typedef struct {
	uint64_t registry_instance;
	uint64_t handle_serial;
} GDExtensionStorageSceneDepthRefV1;

/* knowledge 0 UnknownNotExposed, 1 Known; bytes is zero padding when unknown, never a known-zero cost assertion. */
typedef struct {
	uint64_t bytes;
	uint32_t knowledge;
	uint32_t reserved;
} GDExtensionStorageSceneDepthBytesV1;

/* One original standalone D32_SFLOAT 2D texture, depth 1, one layer/mip/sample, sampling plus depth-stencil attachment usage, and its one original framebuffer child. */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint32_t width;
	uint32_t height;
	uint64_t reserved;
} GDExtensionStorageSceneDepthDescriptorV1;

/* Original allocation origin and finite descriptor. No caller byte quote or native object is accepted. */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionStorageSceneDepthDescriptorV1 descriptor;
	uint32_t allocation_slot;
	uint32_t part;
	uint64_t allocation_generation;
	uint64_t quote_attempt;
} GDExtensionStorageSceneDepthQuoteRequestV1;

/* Accepted original quote transaction, including sticky original quote_error and the same retained facet on native quote failure. Only quote_error OK plus a finite original quote authorizes C claim/Create. Framebuffer owner reservation Known zero is admission accounting policy, never a physical framebuffer cost assertion. Public observations do not grant create authority. */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionStorageSceneDepthRefV1 facet;
	GDExtensionStorageSceneDepthDescriptorV1 descriptor;
	GDExtensionStorageSceneDepthBytesV1 texture_native_requirement;
	GDExtensionStorageSceneDepthBytesV1 texture_owner_reservation;
	GDExtensionStorageSceneDepthBytesV1 framebuffer_owner_reservation;
	GDExtensionStorageSceneDepthBytesV1 framebuffer_driver_backing;
	GDExtensionStorageSceneDepthBytesV1 opaque_driver_overhead;
	uint64_t alignment;
	uint32_t policy;
	uint32_t allocations;
	uint32_t parts;
	uint32_t shared_attachment_references;
	int32_t quote_error;
	uint32_t reserved;
} GDExtensionStorageSceneDepthQuoteV1;

/* Original create transaction resolves facet on the same authenticated bridge; never accepts RIDs, raw private pointers or a reconstructed quote report. */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionStorageSceneDepthRefV1 facet;
	uint32_t allocation_slot;
	uint32_t part;
	uint64_t allocation_generation;
	uint64_t create_attempt;
	uint64_t reserved;
} GDExtensionStorageSceneDepthCreateRequestV1;

/* Original retire transaction resolves facet on the same authenticated bridge; never accepts RIDs, raw private pointers or a reconstructed quote report. */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionStorageSceneDepthRefV1 facet;
	uint32_t allocation_slot;
	uint32_t part;
	uint64_t allocation_generation;
	uint64_t retire_attempt;
	uint64_t reserved;
} GDExtensionStorageSceneDepthRetireRequestV1;

/* Original retained custody/physical-disposal report. API OK means typed transaction delivery only; creation_error remains original even after real physical return. No producer Complete, refund or OutputReleased claim. */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionStorageSceneDepthRefV1 facet;
	uint64_t allocation_generation;
	uint64_t create_attempt;
	uint64_t retire_attempt;
	uint64_t depth_rid;
	uint64_t depth_serial;
	uint64_t framebuffer_rid;
	uint64_t framebuffer_serial;
	GDExtensionStorageSceneDepthBytesV1 depth_driver_backing;
	GDExtensionStorageSceneDepthBytesV1 framebuffer_driver_backing;
	GDExtensionStorageSceneDepthBytesV1 framebuffer_object;
	GDExtensionStorageSceneDepthBytesV1 attachment_reference;
	GDExtensionStorageSceneDepthBytesV1 opaque_driver_overhead;
	uint64_t revision;
	uint32_t allocation_slot;
	uint32_t part;
	uint32_t state;
	uint32_t custody;
	uint32_t known_created_mask;
	uint32_t depth_state;
	uint32_t framebuffer_state;
	uint32_t all_created_children_returned;
	int32_t creation_error;
	int32_t maintenance_error;
	uint32_t create_outcome;
	uint32_t reserved;
} GDExtensionStorageSceneDepthReportV1;

/* Exact original OBSERVE generation-return tuple. Comparison only; cannot issue a generation or permission. */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t registry_instance;
	uint64_t request_serial;
	uint64_t control_serial;
	uint64_t root_serial;
	uint64_t particles;
	uint64_t generation;
} GDExtensionProductionOutputGenerationRefV1;

/* Original immutable generation return. terminal_kind 0 is None, 1 is GenerationReleased. Native release is not E05 source acknowledgment or U09 code close. Each per-slot native lease remains the original Acquire association; this aggregate creates no new lease. */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t registry_instance;
	uint64_t request_serial;
	uint64_t control_serial;
	uint64_t root_serial;
	uint64_t particles;
	uint64_t generation;
	GDExtensionProductionReportV2 root_source;
	uint32_t unused_slots;
	uint32_t returned_slots;
	uint32_t terminal_kind;
	uint32_t native_released;
} GDExtensionProductionOutputGenerationResultV1;

/* Version 3 observation projection for the original V2 request contract. Returned generation terminal is peeked until exact consume; no draw or new physical context is required. */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionProductionReportV2 report;
	GDExtensionProductionOutputGenerationResultV1 output_generation;
} GDExtensionProductionObserveResultV3;

/* Immutable row-major 3x4 affine copied from the original Runtime HostControlService authenticated sample/context. Token bytes are opaque correlation; this transport issues no Host, U05 or native view authority. Runtime retains its original source/sample pins. */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionOutputTokenV1 host_token;
	float affine[12];
} GDExtensionProductionHostTransformInputV1;

/* Immutable physical scene values projected by the original E09 Pending source. Locators grant no logical source identity, phase or native append authority. */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t instance;
	uint64_t particles;
	uint64_t generation;
	uint64_t scenario;
	GDExtensionProductionHostTransformInputV1 host;
} GDExtensionProductionOutputSceneInputV1;

/* Original paid-action Host affine and finite inverse. Values are immutable after preparation and valid only with the original selected request/action/source. No token-to-matrix decoding is performed. */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionOutputTokenV1 host_token;
	float affine[12];
	float inverse[12];
} GDExtensionProductionHostTransformV1;

/* Exact original READ use and allocation generation for one guard semantic block. word_offset is an absolute word offset within the original storage buffer and must fall inside the named READ use; copied locators do not create an E05/native read grant. */
typedef struct {
	uint32_t resource_slot;
	uint32_t use_index;
	uint64_t generation;
	uint64_t word_offset;
} GDExtensionProductionOutputGuardReadV1;

/* Frozen original same-request Acquire, FinalizeGuardCopy and Seal native rows. Original shader rows stay between Acquire and Finalize and alone establish shader coverage. Five READ semantic entries are SourceCount, CountValidity, AttributesValidity, DenseIdsValidity and InstanceGate in that order. Source space is the original WORLD=1 or LOCAL=2. Limits are explicit original prepared native-work admission and consume existing request arena/graph/native/owned-resource capacity; these fields neither allocate additional capacity nor grant source authority. */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionProductionOutputPrepareV1 acquire;
	uint64_t finalize_action_ordinal;
	uint64_t finalize_canonical_suboperation;
	uint64_t seal_action_ordinal;
	uint64_t seal_canonical_suboperation;
	uint32_t finalize_source_recipe_substep;
	uint32_t seal_source_recipe_substep;
	uint32_t source_space;
	uint32_t reserved;
	GDExtensionProductionOutputGuardReadV1 reads[5];
	uint64_t guard_work_limit;
	uint32_t guard_dispatch_limit;
	uint32_t guard_graph_limit;
	uint32_t guard_native_limit;
	uint32_t guard_owned_resource_limit;
} GDExtensionProductionOutputPlanV1;

/* Original FinalizeGuardCopy receipt. A nonzero record token is issued only after actual ordered original guard/copy recording and closed lists; it does not prove GPU completion. Partial/unknown prefixes retain their original custody. Seal consumes that exact same-request result. */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionOutputStatusV1 status;
	uint32_t state;
	uint32_t custody;
	GDExtensionOutputTokenV1 record_token;
} GDExtensionProductionOutputFinalizeResultV1;

/* Original owner ordinary callback before admission seal. One bounded pass; stop new query scheduling and keep stopped after OK. Writes actual pending query/input/result pins. Zero is not Callable free or renderer completion. No borrowed preflight or cleanup authority. */
typedef int32_t (*GDExtensionDeviceExitPrepareFnV1)(void *userdata, uint32_t close_route, uint64_t *out_pending);
/* Original HostControlService approved camera/viewport association. Same admitted library and paid action only. Runtime retains actual U05 typed custody; RID values do not prove it. */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t viewport_rid;
	uint64_t camera_rid;
} GDExtensionProductionHostViewInputV1;

/* Original native viewport identity with exact retained immutable source sequence. Zero sequence/ordinal before first sample. Closed/cancelled association fails; does not issue U05 or GPU completion. */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t viewport_rid;
	uint64_t camera_rid;
	uint64_t view_id;
	uint64_t view_epoch;
	uint64_t present_seq;
	uint64_t capture_ordinal;
} GDExtensionProductionHostViewAssociationV1;

/* Version 2 correlation selector into the original request. It grants no source, view, native receipt or capacity. Core reads all frozen selection facts from that same original action. */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t action_ordinal;
} GDExtensionOutputConsumedSelectorV2;

/* Version 2 exact locator for one original pre-reserved ViewCell and original request pin. Every field is revalidated against original owner state; the POD is neither a grant nor independent custody. */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t registry_instance;
	uint64_t request_serial;
	uint64_t borrow_control_serial;
	uint64_t action_ordinal;
	uint64_t particles;
	uint64_t generation;
	uint64_t lease_serial;
	uint64_t view_cell_serial;
	uint32_t slot_index;
	uint32_t view_cell_index;
	GDExtensionOutputTokenV1 lease_token;
	GDExtensionOutputTokenV1 original_color_token;
} GDExtensionOutputConsumedProofRefV2;

/* Version 2 immutable grouped projection of actual final native color consumption. Full CameraCapture, CameraData, original native read and host snapshots remain behind the retained original ViewCell. Previous host is the frozen slot transform and has no independently minted host token. */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionOutputConsumedProofRefV2 ref;
	GDExtensionOutputAcquireInputV1 original_acquire;
	GDExtensionProductionCameraSampleV1 original_camera;
	GDExtensionProductionHostTransformV1 current_host;
	GDExtensionProductionHostTransformV1 previous_host;
} GDExtensionOutputConsumedProofV2;

/* Version 2 original paid OBSERVE projection. Fresh-zero this output before every call; a complete ABI2 header distinguishes a projection written by this call. Successful borrow remains RETURNED until exact read; every failed borrow projection, including ERR_BUSY, is already OWNER_CONSUMED. Native pin survives exact read until explicit paid return. Errors never imply GPU, source, RECORD or segment completion. */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionDeviceSessionResultV1 result;
	GDExtensionProductionReportV2 report;
	GDExtensionOutputConsumedProofV2 proof;
} GDExtensionOutputConsumedBorrowResultV2;

/* Exact prior consumed proof plus one already accepted READ resource/use locator. Does not import a caller RID. The fixed paid OBSERVE retains the original view/pin on RT before normal output preparation may succeed. All uses of this source must remain READ; aligned previous output remains independently writable. */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionOutputConsumedProofRefV2 proof;
	uint32_t resource_slot;
	uint32_t use_index;
} GDExtensionProductionOutputPredecessorV2;

/* Versioned original preparation plus authenticated predecessor READ. Repeated calls service the same immutable fixed control; Busy performs no output append. */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionProductionOutputPrepareV1 prepare;
	GDExtensionProductionOutputPredecessorV2 predecessor;
} GDExtensionProductionOutputPrepareV2;

/* Versioned original Acquire/Finalize/Seal plan plus authenticated predecessor READ. It keeps existing writable role and guard binding semantics. */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionProductionOutputPlanV1 plan;
	GDExtensionProductionOutputPredecessorV2 predecessor;
} GDExtensionProductionOutputPlanV2;

/* Locator into one original request OUTPUT_METADATA resource; no independent issuer, registry, native generation or authority. The original selection owner remains in its paid slab until its original RELEASE returns every retained prefix. */
typedef struct {
	GDExtensionProductionRequestRefV1 request;
	uint32_t resource_slot;
	uint32_t reserved;
	uint64_t resource_generation;
} GDExtensionOutputMetadataSelectionRefV2;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint32_t mesh_count;
	uint32_t material_count;
} GDExtensionOutputMetadataSelectionQueryV2;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t physical_bytes;
	uint32_t alignment;
	uint32_t mesh_count;
	uint32_t material_count;
	uint32_t reserved;
} GDExtensionOutputMetadataSelectionRequirementsV2;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint32_t metadata_resource_slot;
	uint32_t reserved;
	GDExtensionOutputRidSpanV1 meshes;
	GDExtensionOutputRidSpanV1 materials;
} GDExtensionOutputMetadataSelectionPrepareV2;

/* A nonempty selection is returned even when acquisition fails after placement. The caller must close it and drive the same original request RELEASE; failure never discards an acquired native prefix. */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionDeviceSessionResultV1 status;
	GDExtensionOutputMetadataSelectionRefV2 selection;
	uint32_t retained_meshes;
	uint32_t retained_materials;
} GDExtensionOutputMetadataSelectionResultV2;

/* CPU native selection observation only, issued on the original RT under the SAME retained material/mesh selections. No GPU qualification, raster backing, physical surface hold, pipeline custody or readiness is issued. shader_observation_error must be OK before using shader feature fields. */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t mesh;
	uint64_t mesh_revision;
	uint32_t native_surface;
	uint32_t vertex_count;
	uint32_t index_count;
	uint32_t primitive;
	uint64_t format;
	GDExtensionOutputBoundsV1 mesh_bounds;
	uint64_t material;
	uint64_t material_generation;
	uint64_t shader_version;
	int32_t shader_observation_error;
	uint32_t needs_inverse;
	uint32_t zero_displacement;
	uint32_t material_recipe_rejections;
} GDExtensionOutputMetadataSelectionObservationV2;

/* Create using the same original early metadata selections. Validates original attachment, selected assets and native revisions before and after acquiring the original generation holds; old CreateV3 semantics remain unchanged. */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionOutputCreateInputV3 create;
	GDExtensionOutputMetadataSelectionRefV2 selection;
} GDExtensionOutputCreateInputV4;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionSceneOpaque32V1 connection;
	double transform[12];
	GDExtensionOutputBoundsV1 local_envelope;
	GDExtensionOutputBoundsV1 world_query;
	uint64_t bounds_revision;
} GDExtensionOutputSceneQueryCommitV2;

/* Receipt from the actual atomic scene receiver commit; it does not claim paired scene sources are clean or GPU work completed. */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionDeviceSessionResultV1 status;
	uint64_t receiver;
	uint64_t base;
	uint64_t generation;
	uint64_t bounds_revision;
	uint64_t pose_revision;
	uint64_t pair_revision;
	double transform[12];
	GDExtensionOutputBoundsV1 local_envelope;
	GDExtensionOutputBoundsV1 world_query;
} GDExtensionOutputSceneQueryResultV2;

/* Original authenticated exported DeviceSessionKey and exact Output device descriptor. Canonical token is four LE64 words {registry_instance,device_epoch,session,0x3256454454554f47}. This is descriptor equality only, never authority or capability/profile digest; retain original session owner and transport unchanged. */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t registry_instance;
	uint64_t device_epoch;
	uint64_t session;
	GDExtensionOutputTokenV1 device_tag;
} GDExtensionOutputDeviceFactsV2;

/* Explicit original selection observation: profile Instance192=1, Indexed144=2. requested_pass_mask uses sole Output wire flags opaque=1/depth=2/shadow=4/transparent=8/probe=16. Zero observes eligibility only; nonzero requires the entire mask without downgrade. No Early/Late conversion. */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint32_t profile;
	uint32_t surface;
	uint32_t requested_pass_mask;
	uint32_t reserved;
} GDExtensionOutputSelectionFactsInputV2;

/* Grouped current CPU facts from the original retained SelectionV2 owner. Mesh observation is independently usable when pass_status is unavailable; read masks only when pass_status.native_error is OK. geometry_bounds is exact zero for Instance192, not generated Indexed envelope. eligible_pass_mask is native shader classification, selected_pass_mask is only explicit intent, neither grants GPU qualification, draw/window admission or completion. device is projected from this same original attachment device/session; compare to the pre-query exported original session facts before freezing Create. */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionOutputMetadataSelectionRefV2 selection;
	GDExtensionOutputDeviceFactsV2 device;
	uint32_t surface;
	uint32_t profile;
	GDExtensionOutputMetadataSelectionObservationV2 observation;
	GDExtensionOutputBoundsV1 geometry_bounds;
	GDExtensionDeviceSessionResultV1 pass_status;
	uint32_t eligible_pass_mask;
	uint32_t selected_pass_mask;
} GDExtensionOutputSelectionFactsV2;

/* Sole existing ExternalOutput wire bit assignment. These are explicit pass intents/CPU eligibility, never renderer internal enum values, Early/Late phase bits, GPU qualification or physical window authority. */
typedef enum {
	GDEXTENSION_OUTPUT_PASS_OPAQUE_V2 = 1,
	GDEXTENSION_OUTPUT_PASS_DEPTH_V2 = 2,
	GDEXTENSION_OUTPUT_PASS_SHADOW_V2 = 4,
	GDEXTENSION_OUTPUT_PASS_TRANSPARENT_V2 = 8,
	GDEXTENSION_OUTPUT_PASS_PROBE_V2 = 16,
	GDEXTENSION_OUTPUT_PASS_KNOWN_MASK_V2 = 31,
} GDExtensionOutputPassMaskV2;

/* Version 2 exact original request/control/action/canonical-range result selector. It neither schedules native work nor issues a physical window. The original action cell retains the typed result until exact read after original RECORD consume. */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t registry_instance;
	uint64_t request_serial;
	uint64_t control_serial;
	uint64_t action_ordinal;
	uint64_t canonical_begin;
	uint64_t canonical_end;
	uint64_t canonical_suboperation;
	uint32_t source_recipe_substep;
	uint32_t reserved;
} GDExtensionProductionPreparedResultRefV2;

/* Version 2 actual native prepared execution reply, inline in the same original paid action cell. output_kind 0=ordinary append, 1=Acquire, 2=FinalizeGuardCopy, 3=Seal. Inactive result variants are zero. Multiple original actions in one RECORD retain separate replies; no DSO receiver/reply flags or generation-return projection substitutes for this result. */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionProductionPreparedResultRefV2 ref;
	uint32_t action_kind;
	uint32_t output_kind;
	GDExtensionDeviceSessionResultV1 result;
	GDExtensionProductionAppendV2 append;
	GDExtensionOutputAcquireResultV1 acquire;
	GDExtensionProductionOutputFinalizeResultV1 finalize;
	GDExtensionOutputSealResultV1 seal;
} GDExtensionProductionPreparedActionResultV2;

/* Exact-once original RECORD report plus publicly visible actual native service usage. record retains ABI2 and original result/segment facts. offered is the immutable original native stage budget; remaining subtracts cumulative original staged usage, never refills on another control. Used counters describe only this exact control, including failed actual camera completion, not GPU completion or owner application. Caller settles these receipts before another original control. */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionProductionRecordReportV2 record;
	GDExtensionProductionServiceBudgetV1 offered;
	GDExtensionProductionServiceBudgetV1 remaining;
	uint32_t controls_used;
	uint32_t receipts_used;
	uint32_t passes_used;
	uint32_t reserved;
} GDExtensionProductionRecordReportV3;

/* Explicit revision-1 camera-dependent original upload binding. Ordinals refer to already prepared same-request UPLOAD rows, before the Acquire anchor, with exact bytes view24=96, sort18=72, control3=12. UINT64_MAX means absent. Core fills retained original CPU payload only from the actual selected current CameraSample and retained Host immediately before first bound upload. No view/owner authority is issued by these labels. */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t acquire_action_ordinal;
	uint64_t view_upload_action_ordinal;
	uint64_t sort_upload_action_ordinal;
	uint64_t control_upload_action_ordinal;
	uint32_t needs_inverse;
	uint32_t reserved;
} GDExtensionProductionCameraPayloadV1;

/* Exact original native prepared result in actual execution order, with optional segment closed after that result. cursor 0 starts the stream only once after exact original RECORD consume; next_cursor 0 ends it. flags 1=result,2=segment. Reading does not apply or ACK. Bounded O(1) original-arena lookup. */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t cursor;
	uint64_t next_cursor;
	uint32_t flags;
	uint32_t reserved;
	GDExtensionProductionPreparedActionResultV2 result;
	GDExtensionProductionSegmentV2 segment;
} GDExtensionProductionNativeBatchItemV1;

/* Additive exact Height service transport; explicit original native service grant is separate from source/submission authority. ABI version 2, exact containing sizeof and all reserved zero. */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t grant_serial;
	uint64_t deadline_usec;
	uint32_t controls;
	uint32_t receipts;
	uint32_t passes;
	uint32_t reserved;
} GDExtensionQualificationServiceGrantV2;

/* Additive exact Height service transport; explicit original native service grant is separate from source/submission authority. ABI version 2, exact containing sizeof and all reserved zero. */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionStorageSceneDepthRefV1 facet;
	uint64_t operation_serial;
	uint64_t allocation_generation;
	uint64_t expected_frame_version;
	uint32_t allocation_slot;
	uint32_t part;
	uint32_t mode;
	uint32_t reserved;
} GDExtensionSceneDepthServiceRequestV2;

/* Headerless cumulative original grant usage. No refund or source authority. */
typedef struct {
	uint32_t controls;
	uint32_t polls;
	uint32_t frees;
	uint32_t issues;
	uint32_t retains;
	uint32_t releases;
	uint32_t recycles;
	uint32_t passes;
} GDExtensionQualificationServiceUsageV2;

/* Additive exact Height service transport; explicit original native service grant is separate from source/submission authority. ABI version 2, exact containing sizeof and all reserved zero. */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint32_t mode;
	uint32_t state;
	uint64_t operation_serial;
	uint64_t slot_generation;
	uint64_t grant_serial;
	uint64_t execution_serial;
	uint32_t result_bytes;
	uint32_t segment_count;
	GDExtensionDeviceSessionResultV1 call_result;
	GDExtensionQualificationServiceUsageV2 used;
	GDExtensionQualificationServiceGrantV2 remaining;
	uint32_t service_flags;
	uint32_t reserved;
} GDExtensionQualificationServiceObservationV2;

/* Same original operation and ticket. The three original V2 ranges remain unchanged; gates are InputGate, InstanceGate, OutputGate with exact original upload-backed four-byte ranges. No completion or source authority is conveyed. */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionQualificationRecordV3 record;
	uint32_t readback_size;
	uint32_t reserved;
	GDExtensionQualificationReadbackRangeV1 gates[3];
} GDExtensionQualificationMethodRecordV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionQualificationReportV2 record;
	uint32_t readback_size;
	uint32_t reserved;
	GDExtensionQualificationReadbackObservationV1 gates[3];
} GDExtensionQualificationMethodReportV1;

/* Original protected Host policy mask and actual consumed color-token locator only. No caller view/camera/present metadata or GPU read grant. Kind 1 prepares static WithoutPrevious, kind 2 has the genuine prior-read primary and fallback. Reset upload locates the original paid immutable upload; only its first word changes in the real camera invocation. */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint32_t static_mask;
	uint32_t kind;
	GDExtensionOutputTokenV1 previous_color_token;
	uint64_t reset_upload_action;
	uint64_t fallback_reset_upload_action;
} GDExtensionProductionOutputHistoryStaticV2;

/* Read-only projection of the same request history decision. Full mask and priority reason are frozen only at its genuine camera invocation before first logical action; initial stage returns finalized=0. */
typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint32_t finalized;
	uint32_t mask;
	uint32_t reset;
	uint32_t reserved;
} GDExtensionProductionOutputHistoryStateV2;

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
 * Object ID is the original core ObjectDB ID, not a pointer or caller-invented cleanup authority. Only the issuing initialization frame may enroll, before its binding acknowledgment.
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
 * Register the original generation scene-ready callback inside its qualified SCENE initializer before binding acknowledgment. Guarded lookup only; no late effects or enrollment are inferred.
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
 * @name device_session_qualification_observe_v2
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
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationObserveV2)(GDExtensionQualificationBridgeRefV1 bridge, GDExtensionQualificationTicketRefV1 ticket, const GDExtensionQualificationBudgetV1 *budget, GDExtensionQualificationReportV2 *out_report);

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
 * @name device_session_qualification_prepare_uploads_v2
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
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationPrepareUploadsV2)(GDExtensionQualificationBridgeRefV1 bridge, const GDExtensionQualificationUploadPrepareV1 *request, const GDExtensionQualificationUploadPlanV1 *plans, uint32_t plan_count, GDExtensionQualificationTicketRefV1 *out_ticket, GDExtensionQualificationReportV2 *out_report);

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
 * @name device_session_qualification_record_prepared_v2
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
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationRecordPreparedV2)(GDExtensionQualificationBridgeRefV1 bridge, GDExtensionQualificationTicketRefV1 ticket, const GDExtensionQualificationRecordV2 *request, GDExtensionQualificationReportV2 *out_report);

/**
 * @name device_session_qualification_record_prepared_v3
 * @since 4.7
 *
 * Records a prepared qualification ticket with the original operation identity. The V2 record retains its dispatch envelope; original_range_begin/end are the owner-issued canonical range. Requires the original render-thread bridge admission.
 *
 * @param bridge Synchronous in-process bridge.
 * @param ticket Synchronous in-process ticket.
 * @param request Synchronous in-process request.
 * @param out_report Synchronous in-process out report.
 *
 * @return Original native Error/reason observation. A possible effect requires a retained original child/ticket, never an unowned bare error.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationRecordPreparedV3)(GDExtensionQualificationBridgeRefV1 bridge, GDExtensionQualificationTicketRefV1 ticket, const GDExtensionQualificationRecordV3 *request, GDExtensionQualificationReportV2 *out_report);

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

/**
 * @name device_session_qualification_texture_create_v1
 * @since 4.7
 *
 * Original qualification bridge typed resource transaction; numeric observations do not authorize bare RIDs.
 *
 * @param bridge Original typed transaction bridge.
 * @param request Original typed transaction request.
 * @param out_backing Original typed transaction out_backing.
 *
 * @return Original native Error/reason observation. A possible effect requires a retained original child/ticket, never an unowned bare error.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationTextureCreateV1)(GDExtensionQualificationBridgeRefV1 bridge, const GDExtensionQualificationTextureCreateV1 *request, GDExtensionQualificationResourceBackingV1 *out_backing);

/**
 * @name device_session_qualification_sampler_create_v1
 * @since 4.7
 *
 * Original qualification bridge typed resource transaction; numeric observations do not authorize bare RIDs.
 *
 * @param bridge Original typed transaction bridge.
 * @param request Original typed transaction request.
 * @param out_backing Original typed transaction out_backing.
 *
 * @return Original native Error/reason observation. A possible effect requires a retained original child/ticket, never an unowned bare error.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationSamplerCreateV1)(GDExtensionQualificationBridgeRefV1 bridge, const GDExtensionQualificationSamplerCreateV1 *request, GDExtensionQualificationResourceBackingV1 *out_backing);

/**
 * @name device_session_qualification_set_create_v1
 * @since 4.7
 *
 * Original qualification bridge typed resource transaction; numeric observations do not authorize bare RIDs.
 *
 * @param bridge Original typed transaction bridge.
 * @param request Original typed transaction request.
 * @param bindings Original typed transaction bindings.
 * @param binding_count Original typed transaction binding_count.
 * @param out_backing Original typed transaction out_backing.
 *
 * @return Original native Error/reason observation. A possible effect requires a retained original child/ticket, never an unowned bare error.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationSetCreateV1)(GDExtensionQualificationBridgeRefV1 bridge, const GDExtensionQualificationSetCreateV1 *request, const GDExtensionQualificationUniformBindingV1 *bindings, uint32_t binding_count, GDExtensionQualificationResourceBackingV1 *out_backing);

/**
 * @name device_session_qualification_texture_upload_v1
 * @since 4.7
 *
 * Original qualification bridge typed resource transaction; numeric observations do not authorize bare RIDs.
 *
 * @param bridge Original typed transaction bridge.
 * @param request Original typed transaction request.
 * @param pixels Original typed transaction pixels.
 * @param pixel_bytes Original typed transaction pixel_bytes.
 * @param segments Original typed transaction segments.
 * @param segment_capacity Original typed transaction segment_capacity.
 * @param out_report Original typed transaction out_report.
 *
 * @return Original native Error/reason observation. A possible effect requires a retained original child/ticket, never an unowned bare error.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationTextureUploadV1)(GDExtensionQualificationBridgeRefV1 bridge, const GDExtensionQualificationTextureUploadV1 *request, const void *pixels, uint32_t pixel_bytes, GDExtensionQualificationUploadSegmentV1 *segments, uint32_t segment_capacity, GDExtensionQualificationUploadReportV1 *out_report);

/**
 * @name device_session_qualification_resource_retire_v1
 * @since 4.7
 *
 * Original qualification bridge typed resource transaction; numeric observations do not authorize bare RIDs.
 *
 * @param bridge Original typed transaction bridge.
 * @param request Original typed transaction request.
 * @param out_report Original typed transaction out_report.
 *
 * @return Original native Error/reason observation. A possible effect requires a retained original child/ticket, never an unowned bare error.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationResourceRetireV1)(GDExtensionQualificationBridgeRefV1 bridge, const GDExtensionQualificationResourceRetireV1 *request, GDExtensionQualificationResourceRetireReportV1 *out_report);

/**
 * @name device_session_qualification_resource_retire_observe_v1
 * @since 4.7
 *
 * Original qualification bridge typed resource transaction; numeric observations do not authorize bare RIDs.
 *
 * @param bridge Original typed transaction bridge.
 * @param resource Original typed transaction resource.
 * @param budget Original typed transaction budget.
 * @param out_report Original typed transaction out_report.
 *
 * @return Original native Error/reason observation. A possible effect requires a retained original child/ticket, never an unowned bare error.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationResourceRetireObserveV1)(GDExtensionQualificationBridgeRefV1 bridge, GDExtensionQualificationResourceRefV1 resource, const GDExtensionQualificationBudgetV1 *budget, GDExtensionQualificationResourceRetireReportV1 *out_report);

/**
 * @name device_session_qualification_resource_retire_release_v1
 * @since 4.7
 *
 * Original qualification bridge typed resource transaction; numeric observations do not authorize bare RIDs.
 *
 * @param bridge Original typed transaction bridge.
 * @param resource Original typed transaction resource.
 * @param revision Original typed transaction revision.
 *
 * @return Original native Error/reason observation. A possible effect requires a retained original child/ticket, never an unowned bare error.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationResourceRetireReleaseV1)(GDExtensionQualificationBridgeRefV1 bridge, GDExtensionQualificationResourceRefV1 resource, uint64_t revision);

/**
 * @name device_session_bind_host_producer_v1
 * @since 4.7
 *
 * Internal guarded device-session registration import. Existing loader and guarded entry adoption remain required. Native aligned nonaliasing pointers are required; abi version 1, sizeof(GDExtensionDeviceExitAbi), reserved zero.
 *
 * The original Registry owns all handles and metadata. Empty output handles remain empty on failure; nonempty outputs reject unchanged. No label, address, code hold or registration alone grants operation, OAS, evidence-site or native source authority.
 * Binds once inside the authenticated original SCENE initializer. Identical root and code holder retry returns the same registration identity; contradictory rebinding rejects before publication.
 *
 * @param abi ABI version and layout.
 * @param initialize Exact original unacknowledged SCENE initializer context.
 * @param code_hold Original live SCENE cleanup code hold.
 * @param attachment_identity Original Host root address, identity only and never dereferenced by core.
 * @param out_registration Empty output; original Registry retains its metadata until release.
 *
 * @return Existing Godot Error value.
 */
typedef int32_t (*GDExtensionInterfaceDeviceSessionBindHostProducerV1)(const GDExtensionDeviceExitAbi *abi, GDExtensionDeviceExitInitializationLevelContextRef initialize, GDExtensionDeviceExitLibraryCodeHoldRef code_hold, const void *attachment_identity, GDExtensionProducerRegistrationRefV1 *out_registration);

/**
 * @name device_session_clone_producer_registration_v1
 * @since 4.7
 *
 * Internal guarded device-session registration import. Existing loader and guarded entry adoption remain required. Native aligned nonaliasing pointers are required; abi version 1, sizeof(GDExtensionDeviceExitAbi), reserved zero.
 *
 * The original Registry owns all handles and metadata. Empty output handles remain empty on failure; nonempty outputs reject unchanged. No label, address, code hold or registration alone grants operation, OAS, evidence-site or native source authority.
 * Retains only the already-issued original metadata, including after admission revocation. It cannot reopen source admission.
 *
 * @param abi ABI version and layout.
 * @param source Retained original registration handle.
 * @param out_registration Empty output; original Registry retains its metadata until release.
 *
 * @return Existing Godot Error value.
 */
typedef int32_t (*GDExtensionInterfaceDeviceSessionCloneProducerRegistrationV1)(const GDExtensionDeviceExitAbi *abi, GDExtensionProducerRegistrationRefV1 source, GDExtensionProducerRegistrationRefV1 *out_registration);

/**
 * @name device_session_copy_producer_registration_v1
 * @since 4.7
 *
 * Internal guarded device-session registration import. Existing loader and guarded entry adoption remain required. Native aligned nonaliasing pointers are required; abi version 1, sizeof(GDExtensionDeviceExitAbi), reserved zero.
 *
 * The original Registry owns all handles and metadata. Empty output handles remain empty on failure; nonempty outputs reject unchanged. No label, address, code hold or registration alone grants operation, OAS, evidence-site or native source authority.
 * Copies original immutable label, registry instance and library generation while the exact registration remains retained. No DSO pointer or encoder is invoked.
 *
 * @param abi ABI version and layout.
 * @param source Retained original registration handle.
 * @param out_identity Caller initializes embedded abi to version 1, sizeof this structure, reserved zero. Preserved unchanged on failure.
 *
 * @return Existing Godot Error value.
 */
typedef int32_t (*GDExtensionInterfaceDeviceSessionCopyProducerRegistrationV1)(const GDExtensionDeviceExitAbi *abi, GDExtensionProducerRegistrationRefV1 source, GDExtensionProducerRegistrationIdentityV1 *out_identity);

/**
 * @name device_session_release_producer_registration_v1
 * @since 4.7
 *
 * Internal guarded device-session registration import. Existing loader and guarded entry adoption remain required. Native aligned nonaliasing pointers are required; abi version 1, sizeof(GDExtensionDeviceExitAbi), reserved zero.
 *
 * The original Registry owns all handles and metadata. Empty output handles remain empty on failure; nonempty outputs reject unchanged. No label, address, code hold or registration alone grants operation, OAS, evidence-site or native source authority.
 * Returns exactly this retained registration handle once. It does not retire code, native work or any operation obligation. Repeated or wrong-domain release rejects.
 *
 * @param abi ABI version and layout.
 * @param source Retained original registration handle.
 *
 * @return Existing Godot Error value.
 */
typedef int32_t (*GDExtensionInterfaceDeviceSessionReleaseProducerRegistrationV1)(const GDExtensionDeviceExitAbi *abi, GDExtensionProducerRegistrationRefV1 source);

/**
 * @name device_session_system_scalar_record_v1
 * @since 4.7
 *
 * Original System scalar typed dispatch and exact output readback; distinct from qualification capture.
 *
 * @param bridge Original in-process value; all handles are authenticated by Registry.
 * @param request Original in-process value; all handles are authenticated by Registry.
 * @param out_ticket Original in-process value; all handles are authenticated by Registry.
 * @param out_report Original in-process value; all handles are authenticated by Registry.
 *
 * @return Godot Error and reason for the original Registry action.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionSystemScalarRecordV1)(GDExtensionQualificationBridgeRefV1 bridge, const GDExtensionSystemScalarRecordV1 *request, GDExtensionSystemScalarTicketRefV1 *out_ticket, GDExtensionSystemScalarReportV1 *out_report);

/**
 * @name device_session_system_scalar_observe_v1
 * @since 4.7
 *
 * Observe same ticket native completion and readback.
 *
 * @param bridge Original in-process value; all handles are authenticated by Registry.
 * @param ticket Original in-process value; all handles are authenticated by Registry.
 * @param budget Original in-process value; all handles are authenticated by Registry.
 * @param out_report Original in-process value; all handles are authenticated by Registry.
 *
 * @return Godot Error and reason for the original Registry action.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionSystemScalarObserveV1)(GDExtensionQualificationBridgeRefV1 bridge, GDExtensionSystemScalarTicketRefV1 ticket, const GDExtensionQualificationBudgetV1 *budget, GDExtensionSystemScalarReportV1 *out_report);

/**
 * @name device_session_system_scalar_facts_v1
 * @since 4.7
 *
 * Copy ticket facts at exact report revision.
 *
 * @param bridge Original in-process value; all handles are authenticated by Registry.
 * @param ticket Original in-process value; all handles are authenticated by Registry.
 * @param revision Original in-process value; all handles are authenticated by Registry.
 * @param first Original in-process value; all handles are authenticated by Registry.
 * @param capacity Original in-process value; all handles are authenticated by Registry.
 * @param out_facts Original in-process value; all handles are authenticated by Registry.
 * @param out_written Original in-process value; all handles are authenticated by Registry.
 *
 * @return Godot Error and reason for the original Registry action.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionSystemScalarFactsV1)(GDExtensionQualificationBridgeRefV1 bridge, GDExtensionSystemScalarTicketRefV1 ticket, uint64_t revision, uint32_t first, uint32_t capacity, GDExtensionQualificationFactV1 *out_facts, uint32_t *out_written);

/**
 * @name device_session_system_scalar_copy_v1
 * @since 4.7
 *
 * Copy exact completed output words from same ticket.
 *
 * @param bridge Original in-process value; all handles are authenticated by Registry.
 * @param ticket Original in-process value; all handles are authenticated by Registry.
 * @param revision Original in-process value; all handles are authenticated by Registry.
 * @param out_bytes Original in-process value; all handles are authenticated by Registry.
 * @param capacity Original in-process value; all handles are authenticated by Registry.
 * @param out_written Original in-process value; all handles are authenticated by Registry.
 *
 * @return Godot Error and reason for the original Registry action.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionSystemScalarCopyV1)(GDExtensionQualificationBridgeRefV1 bridge, GDExtensionSystemScalarTicketRefV1 ticket, uint64_t revision, void *out_bytes, uint32_t capacity, uint32_t *out_written);

/**
 * @name device_session_system_scalar_cancel_v1
 * @since 4.7
 *
 * Close admission; preserve accepted prefix through physical return.
 *
 * @param bridge Original in-process value; all handles are authenticated by Registry.
 * @param ticket Original in-process value; all handles are authenticated by Registry.
 *
 * @return Godot Error and reason for the original Registry action.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionSystemScalarCancelV1)(GDExtensionQualificationBridgeRefV1 bridge, GDExtensionSystemScalarTicketRefV1 ticket);

/**
 * @name device_session_system_scalar_release_v1
 * @since 4.7
 *
 * Acknowledge ticket only after physical retirement.
 *
 * @param bridge Original in-process value; all handles are authenticated by Registry.
 * @param ticket Original in-process value; all handles are authenticated by Registry.
 *
 * @return Godot Error and reason for the original Registry action.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionSystemScalarReleaseV1)(GDExtensionQualificationBridgeRefV1 bridge, GDExtensionSystemScalarTicketRefV1 ticket);

/**
 * @name device_session_production_quote_v1
 * @since 4.7
 *
 * Original checked Core production physical transport. Availability does not issue original Runtime logical source, operation or window grants.
 *
 * Inputs are synchronous aligned nonaliasing native POD spans. Only the two explicitly installed callbacks and their userdata are retained under the original accepted CodeHold. Production DTO abi.struct_size is the containing DTO sizeof; reused installed qualification DTOs keep their original common header contract.
 *
 * @param abi Synchronous checked abi.
 * @param scene_ready Synchronous checked scene ready.
 * @param producer Synchronous checked producer.
 * @param device_context Synchronous checked device context.
 * @param device_session Synchronous checked device session.
 * @param capacity Synchronous checked capacity.
 * @param out_quote Synchronous checked out quote.
 *
 * @return Original checked native result. Returned API calls and Core physical contexts do not issue original Runtime logical permissions.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionQuoteV1)(const GDExtensionDeviceExitAbi *abi, GDExtensionDeviceExitSceneReadyContextRef scene_ready, GDExtensionProducerRegistrationRefV1 producer, GDExtensionDeviceRegistryContextRef device_context, GDExtensionOriginalDeviceSessionRef device_session, const GDExtensionProductionCapacityV1 *capacity, GDExtensionProductionMetadataQuoteV1 *out_quote);

/**
 * @name device_session_production_accept_v1
 * @since 4.7
 *
 * Original checked Core production physical transport. Availability does not issue original Runtime logical source, operation or window grants.
 *
 * Inputs are synchronous aligned nonaliasing native POD spans. Only the two explicitly installed callbacks and their userdata are retained under the original accepted CodeHold. Production DTO abi.struct_size is the containing DTO sizeof; reused installed qualification DTOs keep their original common header contract.
 *
 * @param abi Synchronous checked abi.
 * @param scene_ready Synchronous checked scene ready.
 * @param producer Synchronous checked producer.
 * @param device_context Synchronous checked device context.
 * @param device_session Synchronous checked device session.
 * @param code_hold Synchronous checked code hold.
 * @param quote Synchronous checked quote.
 * @param out_attachment Synchronous checked out attachment.
 * @param out_report Synchronous checked out report.
 *
 * @return Original checked native result. Returned API calls and Core physical contexts do not issue original Runtime logical permissions.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionAcceptV1)(const GDExtensionDeviceExitAbi *abi, GDExtensionDeviceExitSceneReadyContextRef scene_ready, GDExtensionProducerRegistrationRefV1 producer, GDExtensionDeviceRegistryContextRef device_context, GDExtensionOriginalDeviceSessionRef device_session, GDExtensionDeviceExitLibraryCodeHoldRef code_hold, const GDExtensionProductionMetadataQuoteV1 *quote, GDExtensionProductionAttachmentRefV1 *out_attachment, GDExtensionProductionInstallationReportV1 *out_report);

/**
 * @name device_session_production_install_record_receiver_v1
 * @since 4.7
 *
 * Original checked Core production physical transport. Availability does not issue original Runtime logical source, operation or window grants.
 *
 * Inputs are synchronous aligned nonaliasing native POD spans. Only the two explicitly installed callbacks and their userdata are retained under the original accepted CodeHold. Production DTO abi.struct_size is the containing DTO sizeof; reused installed qualification DTOs keep their original common header contract.
 * Both nonnull callbacks are frozen before Open, under the original accepted CodeHold; the metadata callback uses the same userdata and carries no recording context.
 *
 * @param scene_ready Synchronous checked scene ready.
 * @param attachment Synchronous checked attachment.
 * @param record_receiver Synchronous checked record receiver.
 * @param userdata Synchronous checked userdata.
 * @param metadata_receiver Synchronous checked metadata receiver.
 *
 * @return Original checked native result. Returned API calls and Core physical contexts do not issue original Runtime logical permissions.
 */
typedef int32_t (*GDExtensionInterfaceDeviceSessionProductionInstallRecordReceiverV1)(GDExtensionDeviceExitSceneReadyContextRef scene_ready, GDExtensionProductionAttachmentRefV1 attachment, GDExtensionProductionRecordReceiverV1 record_receiver, void *userdata, GDExtensionProductionMetadataReceiverV1 metadata_receiver);

/**
 * @name device_session_production_reserve_v1
 * @since 4.7
 *
 * Original checked Core production physical transport. Availability does not issue original Runtime logical source, operation or window grants.
 *
 * Inputs are synchronous aligned nonaliasing native POD spans. Only the two explicitly installed callbacks and their userdata are retained under the original accepted CodeHold. Production DTO abi.struct_size is the containing DTO sizeof; reused installed qualification DTOs keep their original common header contract.
 * Operation numbers, preparation and this physical transport are not original Runtime logical grants. The original B/Runtime operation/window exporter is a separate required authority before original business use.
 *
 * @param attachment Synchronous checked attachment.
 * @param operation Synchronous checked operation.
 * @param capacity Synchronous checked capacity.
 * @param actions Synchronous checked actions.
 * @param action_count Synchronous checked action count.
 * @param resources Synchronous checked resources.
 * @param resource_count Synchronous checked resource count.
 * @param uses Synchronous checked uses.
 * @param use_count Synchronous checked use count.
 * @param readbacks Synchronous checked readbacks.
 * @param readback_count Synchronous checked readback count.
 * @param out_request Synchronous checked out request.
 *
 * @return Original checked native result. Returned API calls and Core physical contexts do not issue original Runtime logical permissions.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionReserveV1)(GDExtensionProductionAttachmentRefV1 attachment, const GDExtensionProductionOperationV1 *operation, const GDExtensionProductionCapacityV1 *capacity, const GDExtensionProductionActionV1 *actions, uint32_t action_count, const GDExtensionProductionResourceV1 *resources, uint32_t resource_count, const GDExtensionProductionUseV1 *uses, uint32_t use_count, const GDExtensionProductionReadbackV1 *readbacks, uint32_t readback_count, GDExtensionProductionRequestRefV1 *out_request);

/**
 * @name device_session_production_raster_reserve_v1
 * @since 4.7
 *
 * Checked Core production raster transport. This independent typed entry point reserves no logical Runtime authority and does not reuse dispatch or qualification ownership.
 *
 * @param attachment Synchronous checked typed raster input.
 * @param operation Synchronous checked typed raster input.
 * @param capacity Synchronous checked typed raster input.
 * @param action Synchronous checked typed raster input.
 * @param out_request Synchronous checked typed raster input.
 *
 * @return Checked typed raster result.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionRasterReserveV1)(GDExtensionProductionAttachmentRefV1 attachment, const GDExtensionProductionOperationV1 *operation, const GDExtensionProductionCapacityV1 *capacity, const GDExtensionProductionRasterActionV1 *action, GDExtensionProductionRequestRefV1 *out_request);

/**
 * @name device_session_production_record_schedule_v1
 * @since 4.7
 *
 * Original checked Core production physical transport. Availability does not issue original Runtime logical source, operation or window grants.
 *
 * Inputs are synchronous aligned nonaliasing native POD spans. Only the two explicitly installed callbacks and their userdata are retained under the original accepted CodeHold. Production DTO abi.struct_size is the containing DTO sizeof; reused installed qualification DTOs keep their original common header contract.
 * Operation numbers, preparation and this physical transport are not original Runtime logical grants. The original B/Runtime operation/window exporter is a separate required authority before original business use.
 *
 * @param request Synchronous checked request.
 * @param out_control_serial Synchronous checked out control serial.
 *
 * @return Original checked native result. Returned API calls and Core physical contexts do not issue original Runtime logical permissions.
 */
typedef int32_t (*GDExtensionInterfaceDeviceSessionProductionRecordScheduleV1)(GDExtensionProductionRequestRefV1 request, uint64_t *out_control_serial);

/**
 * @name device_session_production_output_cancel_schedule_v1
 * @since 4.7
 *
 * Schedule cleanup-only RECORD on the original retained Acquire request and fixed control cell. Requires its installed original SLOT owner. The protected receiver may cancel that exact slot; all append/new-work APIs are denied. Consume and ACK use the existing original RECORD interfaces. No cancellation reply proves native slot return.
 *
 * @param request Synchronous checked request.
 * @param budget Original finite cleanup control/pass/receipt authorization, retained unchanged in the same fixed cell.
 * @param out_control_serial Synchronous checked out control serial.
 *
 * @return Original checked native result. Returned API calls and Core physical contexts do not issue original Runtime logical permissions.
 */
typedef int32_t (*GDExtensionInterfaceDeviceSessionProductionOutputCancelScheduleV1)(GDExtensionProductionRequestRefV1 request, const GDExtensionProductionServiceBudgetV1 *budget, uint64_t *out_control_serial);

/**
 * @name device_session_production_record_consume_v1
 * @since 4.7
 *
 * Original checked Core production physical transport. Availability does not issue original Runtime logical source, operation or window grants.
 *
 * Inputs are synchronous aligned nonaliasing native POD spans. Only the two explicitly installed callbacks and their userdata are retained under the original accepted CodeHold. Production DTO abi.struct_size is the containing DTO sizeof; reused installed qualification DTOs keep their original common header contract.
 * Consumes exactly one returned original RECORD control serial; Pending/Queued/Executing returns Busy without consuming. Duplicate consume fails; owner retains the copied immutable receipt.
 *
 * @param request Synchronous checked request.
 * @param control_serial Synchronous checked control serial.
 * @param out_report Synchronous checked out report.
 *
 * @return Original checked native result. Returned API calls and Core physical contexts do not issue original Runtime logical permissions.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionRecordConsumeV1)(GDExtensionProductionRequestRefV1 request, uint64_t control_serial, GDExtensionProductionRecordReportV1 *out_report);

/**
 * @name device_session_production_append_v1
 * @since 4.7
 *
 * Original checked Core production physical transport. Availability does not issue original Runtime logical source, operation or window grants.
 *
 * Inputs are synchronous aligned nonaliasing native POD spans. Only the two explicitly installed callbacks and their userdata are retained under the original accepted CodeHold. Production DTO abi.struct_size is the containing DTO sizeof; reused installed qualification DTOs keep their original common header contract.
 * Operation numbers, preparation and this physical transport are not original Runtime logical grants. The original B/Runtime operation/window exporter is a separate required authority before original business use.
 *
 * @param physical_context Synchronous checked physical context.
 * @param request Synchronous checked request.
 * @param canonical_suboperation Synchronous checked canonical suboperation.
 * @param revision Synchronous checked revision.
 * @param out_append Synchronous checked out append.
 *
 * @return Original checked native result. Returned API calls and Core physical contexts do not issue original Runtime logical permissions.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionAppendV1)(GDExtensionProductionPhysicalContextRefV1 physical_context, GDExtensionProductionRequestRefV1 request, uint64_t canonical_suboperation, uint64_t revision, GDExtensionProductionAppendV1 *out_append);

/**
 * @name device_session_production_close_segment_v1
 * @since 4.7
 *
 * Original checked Core production physical transport. Availability does not issue original Runtime logical source, operation or window grants.
 *
 * Inputs are synchronous aligned nonaliasing native POD spans. Only the two explicitly installed callbacks and their userdata are retained under the original accepted CodeHold. Production DTO abi.struct_size is the containing DTO sizeof; reused installed qualification DTOs keep their original common header contract.
 *
 * @param physical_context Synchronous checked physical context.
 * @param request Synchronous checked request.
 * @param out_segment Synchronous checked out segment.
 *
 * @return Original checked native result. Returned API calls and Core physical contexts do not issue original Runtime logical permissions.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionCloseSegmentV1)(GDExtensionProductionPhysicalContextRefV1 physical_context, GDExtensionProductionRequestRefV1 request, GDExtensionProductionSegmentV1 *out_segment);

/**
 * @name device_session_production_ack_segment_v1
 * @since 4.7
 *
 * Original checked Core production physical transport. Availability does not issue original Runtime logical source, operation or window grants.
 *
 * Inputs are synchronous aligned nonaliasing native POD spans. Only the two explicitly installed callbacks and their userdata are retained under the original accepted CodeHold. Production DTO abi.struct_size is the containing DTO sizeof; reused installed qualification DTOs keep their original common header contract.
 * Requires an actual authenticated original owner entry, including the fresh Core accepted-metadata lane after seal/cancel/quiesce; a request handle or owner thread alone is insufficient.
 *
 * @param request Synchronous checked request.
 * @param segment Synchronous checked segment.
 * @param segment_revision Synchronous checked segment revision.
 *
 * @return Original checked native result. Returned API calls and Core physical contexts do not issue original Runtime logical permissions.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionAckSegmentV1)(GDExtensionProductionRequestRefV1 request, uint64_t segment, uint64_t segment_revision);

/**
 * @name device_session_production_open_v1
 * @since 4.7
 *
 * Original checked Core production physical transport. Availability does not issue original Runtime logical source, operation or window grants.
 *
 * Inputs are synchronous aligned nonaliasing native POD spans. Only the two explicitly installed callbacks and their userdata are retained under the original accepted CodeHold. Production DTO abi.struct_size is the containing DTO sizeof; reused installed qualification DTOs keep their original common header contract.
 *
 * @param scene_ready Synchronous checked scene ready.
 * @param attachment Synchronous checked attachment.
 *
 * @return Original checked native result. Returned API calls and Core physical contexts do not issue original Runtime logical permissions.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionOpenV1)(GDExtensionDeviceExitSceneReadyContextRef scene_ready, GDExtensionProductionAttachmentRefV1 attachment);

/**
 * @name device_session_production_set_create_v1
 * @since 4.7
 *
 * Original checked Core production physical transport. Availability does not issue original Runtime logical source, operation or window grants.
 *
 * Inputs are synchronous aligned nonaliasing native POD spans. Only the two explicitly installed callbacks and their userdata are retained under the original accepted CodeHold. Production DTO abi.struct_size is the containing DTO sizeof; reused installed qualification DTOs keep their original common header contract.
 *
 * @param request Synchronous checked request.
 * @param resource_slot Synchronous checked resource slot.
 * @param unit_slot Synchronous checked unit slot.
 * @param shader_set Synchronous checked shader set.
 * @param bindings Synchronous checked bindings.
 * @param binding_count Synchronous checked binding count.
 *
 * @return Original checked native result. Returned API calls and Core physical contexts do not issue original Runtime logical permissions.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionSetCreateV1)(GDExtensionProductionRequestRefV1 request, uint32_t resource_slot, uint32_t unit_slot, uint32_t shader_set, const GDExtensionProductionSetBindingV1 *bindings, uint32_t binding_count);

/**
 * @name device_session_production_storage_create_v1
 * @since 4.7
 *
 * Original checked Core production physical transport. Availability does not issue original Runtime logical source, operation or window grants.
 *
 * Inputs are synchronous aligned nonaliasing native POD spans. Only the two explicitly installed callbacks and their userdata are retained under the original accepted CodeHold. Production DTO abi.struct_size is the containing DTO sizeof; reused installed qualification DTOs keep their original common header contract.
 *
 * @param request Synchronous checked request.
 * @param resource_slot Synchronous checked resource slot.
 * @param origin Synchronous checked origin.
 * @param out_backing Synchronous checked out backing.
 *
 * @return Original checked native result. Returned API calls and Core physical contexts do not issue original Runtime logical permissions.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionStorageCreateV1)(GDExtensionProductionRequestRefV1 request, uint32_t resource_slot, const GDExtensionStorageCreateRequestV1 *origin, GDExtensionStorageBackingV1 *out_backing);

/**
 * @name device_session_production_storage_retire_v1
 * @since 4.7
 *
 * Original checked Core production physical transport. Availability does not issue original Runtime logical source, operation or window grants.
 *
 * Inputs are synchronous aligned nonaliasing native POD spans. Only the two explicitly installed callbacks and their userdata are retained under the original accepted CodeHold. Production DTO abi.struct_size is the containing DTO sizeof; reused installed qualification DTOs keep their original common header contract.
 *
 * @param request Synchronous checked request.
 * @param resource_slot Synchronous checked resource slot.
 * @param origin Synchronous checked origin.
 * @param out_retire Synchronous checked out retire.
 * @param out_report Synchronous checked out report.
 *
 * @return Original checked native result. Returned API calls and Core physical contexts do not issue original Runtime logical permissions.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionStorageRetireV1)(GDExtensionProductionRequestRefV1 request, uint32_t resource_slot, const GDExtensionStorageRetireRequestV1 *origin, GDExtensionStorageRetireRefV1 *out_retire, GDExtensionStorageRetireReportV1 *out_report);

/**
 * @name device_session_production_storage_observe_v1
 * @since 4.7
 *
 * Original checked Core production physical transport. Availability does not issue original Runtime logical source, operation or window grants.
 *
 * Inputs are synchronous aligned nonaliasing native POD spans. Only the two explicitly installed callbacks and their userdata are retained under the original accepted CodeHold. Production DTO abi.struct_size is the containing DTO sizeof; reused installed qualification DTOs keep their original common header contract.
 *
 * @param request Synchronous checked request.
 * @param retire Synchronous checked retire.
 * @param budget Synchronous checked budget.
 * @param out_report Synchronous checked out report.
 *
 * @return Original checked native result. Returned API calls and Core physical contexts do not issue original Runtime logical permissions.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionStorageObserveV1)(GDExtensionProductionRequestRefV1 request, GDExtensionStorageRetireRefV1 retire, const GDExtensionProductionServiceBudgetV1 *budget, GDExtensionStorageRetireReportV1 *out_report);

/**
 * @name device_session_production_storage_release_v1
 * @since 4.7
 *
 * Original checked Core production physical transport. Availability does not issue original Runtime logical source, operation or window grants.
 *
 * Inputs are synchronous aligned nonaliasing native POD spans. Only the two explicitly installed callbacks and their userdata are retained under the original accepted CodeHold. Production DTO abi.struct_size is the containing DTO sizeof; reused installed qualification DTOs keep their original common header contract.
 *
 * @param request Synchronous checked request.
 * @param retire Synchronous checked retire.
 *
 * @return Original checked native result. Returned API calls and Core physical contexts do not issue original Runtime logical permissions.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionStorageReleaseV1)(GDExtensionProductionRequestRefV1 request, GDExtensionStorageRetireRefV1 retire);

/**
 * @name device_session_production_import_unit_v1
 * @since 4.7
 *
 * Original checked Core production physical transport. Availability does not issue original Runtime logical source, operation or window grants.
 *
 * Inputs are synchronous aligned nonaliasing native POD spans. Only the two explicitly installed callbacks and their userdata are retained under the original accepted CodeHold. Production DTO abi.struct_size is the containing DTO sizeof; reused installed qualification DTOs keep their original common header contract.
 *
 * @param request Synchronous checked request.
 * @param resource_slot Synchronous checked resource slot.
 * @param backing Synchronous checked backing.
 *
 * @return Original checked native result. Returned API calls and Core physical contexts do not issue original Runtime logical permissions.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionImportUnitV1)(GDExtensionProductionRequestRefV1 request, uint32_t resource_slot, const GDExtensionNativeUnitBackingV1 *backing);

/**
 * @name device_session_production_import_storage_v1
 * @since 4.7
 *
 * Original checked Core production physical transport. Availability does not issue original Runtime logical source, operation or window grants.
 *
 * Inputs are synchronous aligned nonaliasing native POD spans. Only the two explicitly installed callbacks and their userdata are retained under the original accepted CodeHold. Production DTO abi.struct_size is the containing DTO sizeof; reused installed qualification DTOs keep their original common header contract.
 *
 * @param request Synchronous checked request.
 * @param resource_slot Synchronous checked resource slot.
 * @param backing Synchronous checked backing.
 *
 * @return Original checked native result. Returned API calls and Core physical contexts do not issue original Runtime logical permissions.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionImportStorageV1)(GDExtensionProductionRequestRefV1 request, uint32_t resource_slot, const GDExtensionStorageBackingV1 *backing);

/**
 * @name device_session_production_import_set_v1
 * @since 4.7
 *
 * Original checked Core production physical transport. Availability does not issue original Runtime logical source, operation or window grants.
 *
 * Inputs are synchronous aligned nonaliasing native POD spans. Only the two explicitly installed callbacks and their userdata are retained under the original accepted CodeHold. Production DTO abi.struct_size is the containing DTO sizeof; reused installed qualification DTOs keep their original common header contract.
 *
 * @param request Synchronous checked request.
 * @param resource_slot Synchronous checked resource slot.
 * @param backing Synchronous checked backing.
 * @param unit_slot Synchronous checked unit slot.
 * @param shader_set Synchronous checked shader set.
 * @param bindings Synchronous checked bindings.
 * @param binding_count Synchronous checked binding count.
 *
 * @return Original checked native result. Returned API calls and Core physical contexts do not issue original Runtime logical permissions.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionImportSetV1)(GDExtensionProductionRequestRefV1 request, uint32_t resource_slot, const GDExtensionQualificationResourceBackingV1 *backing, uint32_t unit_slot, uint32_t shader_set, const GDExtensionProductionSetBindingV1 *bindings, uint32_t binding_count);

/**
 * @name device_session_production_prepare_dispatch_v1
 * @since 4.7
 *
 * Original checked Core production physical transport. Availability does not issue original Runtime logical source, operation or window grants.
 *
 * Inputs are synchronous aligned nonaliasing native POD spans. Only the two explicitly installed callbacks and their userdata are retained under the original accepted CodeHold. Production DTO abi.struct_size is the containing DTO sizeof; reused installed qualification DTOs keep their original common header contract.
 *
 * @param request Synchronous checked request.
 * @param dispatch Synchronous checked dispatch.
 * @param push Synchronous checked push.
 * @param push_bytes Synchronous checked push bytes.
 *
 * @return Original checked native result. Returned API calls and Core physical contexts do not issue original Runtime logical permissions.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionPrepareDispatchV1)(GDExtensionProductionRequestRefV1 request, const GDExtensionProductionDispatchV1 *dispatch, const void *push, uint32_t push_bytes);

/**
 * @name device_session_production_prepare_copy_v1
 * @since 4.7
 *
 * Original checked Core production physical transport. Availability does not issue original Runtime logical source, operation or window grants.
 *
 * Inputs are synchronous aligned nonaliasing native POD spans. Only the two explicitly installed callbacks and their userdata are retained under the original accepted CodeHold. Production DTO abi.struct_size is the containing DTO sizeof; reused installed qualification DTOs keep their original common header contract.
 *
 * @param request Synchronous checked request.
 * @param copy Synchronous checked copy.
 *
 * @return Original checked native result. Returned API calls and Core physical contexts do not issue original Runtime logical permissions.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionPrepareCopyV1)(GDExtensionProductionRequestRefV1 request, const GDExtensionProductionCopyV1 *copy);

/**
 * @name device_session_production_prepare_upload_v1
 * @since 4.7
 *
 * Original checked Core production physical transport. Availability does not issue original Runtime logical source, operation or window grants.
 *
 * Inputs are synchronous aligned nonaliasing native POD spans. Only the two explicitly installed callbacks and their userdata are retained under the original accepted CodeHold. Production DTO abi.struct_size is the containing DTO sizeof; reused installed qualification DTOs keep their original common header contract.
 *
 * @param request Synchronous checked request.
 * @param upload Synchronous checked upload.
 * @param payload Synchronous checked payload.
 * @param payload_bytes Synchronous checked payload bytes.
 *
 * @return Original checked native result. Returned API calls and Core physical contexts do not issue original Runtime logical permissions.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionPrepareUploadV1)(GDExtensionProductionRequestRefV1 request, const GDExtensionProductionUploadV1 *upload, const void *payload, uint64_t payload_bytes);

/**
 * @name device_session_production_prepare_readback_v1
 * @since 4.7
 *
 * Original checked Core production physical transport. Availability does not issue original Runtime logical source, operation or window grants.
 *
 * Inputs are synchronous aligned nonaliasing native POD spans. Only the two explicitly installed callbacks and their userdata are retained under the original accepted CodeHold. Production DTO abi.struct_size is the containing DTO sizeof; reused installed qualification DTOs keep their original common header contract.
 *
 * @param request Synchronous checked request.
 * @param canonical_suboperation Synchronous checked canonical suboperation.
 * @param readback_slot Synchronous checked readback slot.
 *
 * @return Original checked native result. Returned API calls and Core physical contexts do not issue original Runtime logical permissions.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionPrepareReadbackV1)(GDExtensionProductionRequestRefV1 request, uint64_t canonical_suboperation, uint32_t readback_slot);

/**
 * @name device_session_production_observe_v1
 * @since 4.7
 *
 * Original checked Core production physical transport. Availability does not issue original Runtime logical source, operation or window grants.
 *
 * Inputs are synchronous aligned nonaliasing native POD spans. Only the two explicitly installed callbacks and their userdata are retained under the original accepted CodeHold. Production DTO abi.struct_size is the containing DTO sizeof; reused installed qualification DTOs keep their original common header contract.
 *
 * @param request Synchronous checked request.
 * @param budget Synchronous checked budget.
 * @param out_report Synchronous checked out report.
 *
 * @return Original checked native result. Returned API calls and Core physical contexts do not issue original Runtime logical permissions.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionObserveV1)(GDExtensionProductionRequestRefV1 request, const GDExtensionProductionServiceBudgetV1 *budget, GDExtensionProductionReportV1 *out_report);

/**
 * @name device_session_production_facts_v1
 * @since 4.7
 *
 * Original checked Core production physical transport. Availability does not issue original Runtime logical source, operation or window grants.
 *
 * Inputs are synchronous aligned nonaliasing native POD spans. Only the two explicitly installed callbacks and their userdata are retained under the original accepted CodeHold. Production DTO abi.struct_size is the containing DTO sizeof; reused installed qualification DTOs keep their original common header contract.
 *
 * @param request Synchronous checked request.
 * @param revision Synchronous checked revision.
 * @param first Synchronous checked first.
 * @param out_facts Synchronous checked out facts.
 * @param fact_capacity Synchronous checked fact capacity.
 * @param out_written Synchronous checked out written.
 *
 * @return Original checked native result. Returned API calls and Core physical contexts do not issue original Runtime logical permissions.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionFactsV1)(GDExtensionProductionRequestRefV1 request, uint64_t revision, uint32_t first, GDExtensionProductionFactV1 *out_facts, uint32_t fact_capacity, uint32_t *out_written);

/**
 * @name device_session_production_copy_v1
 * @since 4.7
 *
 * Original checked Core production physical transport. Availability does not issue original Runtime logical source, operation or window grants.
 *
 * Inputs are synchronous aligned nonaliasing native POD spans. Only the two explicitly installed callbacks and their userdata are retained under the original accepted CodeHold. Production DTO abi.struct_size is the containing DTO sizeof; reused installed qualification DTOs keep their original common header contract.
 *
 * @param request Synchronous checked request.
 * @param revision Synchronous checked revision.
 * @param readback_slot Synchronous checked readback slot.
 * @param destination Synchronous checked destination.
 * @param destination_capacity Synchronous checked destination capacity.
 * @param out_written Synchronous checked out written.
 *
 * @return Original checked native result. Returned API calls and Core physical contexts do not issue original Runtime logical permissions.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionCopyV1)(GDExtensionProductionRequestRefV1 request, uint64_t revision, uint32_t readback_slot, void *destination, uint64_t destination_capacity, uint64_t *out_written);

/**
 * @name device_session_production_cancel_v1
 * @since 4.7
 *
 * Original checked Core production physical transport. Availability does not issue original Runtime logical source, operation or window grants.
 *
 * Inputs are synchronous aligned nonaliasing native POD spans. Only the two explicitly installed callbacks and their userdata are retained under the original accepted CodeHold. Production DTO abi.struct_size is the containing DTO sizeof; reused installed qualification DTOs keep their original common header contract.
 *
 * @param request Synchronous checked request.
 *
 * @return Original checked native result. Returned API calls and Core physical contexts do not issue original Runtime logical permissions.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionCancelV1)(GDExtensionProductionRequestRefV1 request);

/**
 * @name device_session_production_release_v1
 * @since 4.7
 *
 * Original checked Core production physical transport. Availability does not issue original Runtime logical source, operation or window grants.
 *
 * Inputs are synchronous aligned nonaliasing native POD spans. Only the two explicitly installed callbacks and their userdata are retained under the original accepted CodeHold. Production DTO abi.struct_size is the containing DTO sizeof; reused installed qualification DTOs keep their original common header contract.
 * Native aligned separate input/output spans are required. Zero/expired fresh input only queries and never publishes. Only actual one-cell controls and aggregate RT/owner passes/receipts are subtracted. Owned tail cursor maintenance shares this same explicit slice before final owner reclaim.
 *
 * @param request Original accepted request.
 * @param input Explicit physical service slice, copied into the same fixed cell before publication; never a Runtime logical grant.
 * @param out_remaining Original ABI/header/deadline. Pending returns zero available counts while the full slice remains escrowed. Returned reports only the old cell actual remaining; discard unused old balance, never credit a new meter.
 * @param out_report Actual immutable original result/report plus fixed service serial/pending state. Returned never accepts a fresh turn in the same call.
 *
 * @return Original checked native result. Returned API calls and Core physical contexts do not issue original Runtime logical permissions.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionReleaseV1)(GDExtensionProductionRequestRefV1 request, const GDExtensionProductionServiceBudgetV1 *input, GDExtensionProductionServiceBudgetV1 *out_remaining, GDExtensionProductionReportV1 *out_report);

/**
 * @name device_session_production_close_v1
 * @since 4.7
 *
 * Original checked Core production physical transport. Availability does not issue original Runtime logical source, operation or window grants.
 *
 * Inputs are synchronous aligned nonaliasing native POD spans. Only the two explicitly installed callbacks and their userdata are retained under the original accepted CodeHold. Production DTO abi.struct_size is the containing DTO sizeof; reused installed qualification DTOs keep their original common header contract.
 *
 * @param attachment Synchronous checked attachment.
 *
 * @return Original checked native result. Returned API calls and Core physical contexts do not issue original Runtime logical permissions.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionCloseV1)(GDExtensionProductionAttachmentRefV1 attachment);

/**
 * @name device_session_production_installation_observe_v1
 * @since 4.7
 *
 * Original checked Core production physical transport. Availability does not issue original Runtime logical source, operation or window grants.
 *
 * Inputs are synchronous aligned nonaliasing native POD spans. Only the two explicitly installed callbacks and their userdata are retained under the original accepted CodeHold. Production DTO abi.struct_size is the containing DTO sizeof; reused installed qualification DTOs keep their original common header contract.
 *
 * @param attachment Synchronous checked attachment.
 * @param budget Synchronous checked budget.
 * @param out_report Synchronous checked out report.
 *
 * @return Original checked native result. Returned API calls and Core physical contexts do not issue original Runtime logical permissions.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionInstallationObserveV1)(GDExtensionProductionAttachmentRefV1 attachment, const GDExtensionProductionServiceBudgetV1 *budget, GDExtensionProductionInstallationReportV1 *out_report);

/**
 * @name device_session_qualification_raster_unit_create_v1
 * @since 4.7
 *
 * Original finite bridge-owned depth raster child. Shader, render pipeline and internal framebuffer retain separate actual physical return facts; observed RIDs never authorize admission.
 *
 * @param bridge Synchronous in-process bridge.
 * @param request Synchronous in-process request.
 * @param container_bytes Synchronous in-process container bytes.
 * @param byte_count Synchronous in-process byte count.
 * @param out_backing Synchronous in-process out backing.
 *
 * @return Original native Error/reason observation. A possible effect requires a retained original child/ticket, never an unowned bare error.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationRasterUnitCreateV1)(GDExtensionQualificationBridgeRefV1 bridge, const GDExtensionQualificationRasterCreateV1 *request, const void *container_bytes, uint64_t byte_count, GDExtensionQualificationRasterBackingV1 *out_backing);

/**
 * @name device_session_qualification_raster_unit_retire_v1
 * @since 4.7
 *
 * Original finite bridge-owned depth raster child. Shader, render pipeline and internal framebuffer retain separate actual physical return facts; observed RIDs never authorize admission.
 *
 * @param bridge Synchronous in-process bridge.
 * @param request Synchronous in-process request.
 * @param out_retire Synchronous in-process out retire.
 * @param out_report Synchronous in-process out report.
 *
 * @return Original native Error/reason observation. A possible effect requires a retained original child/ticket, never an unowned bare error.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationRasterUnitRetireV1)(GDExtensionQualificationBridgeRefV1 bridge, const GDExtensionQualificationRasterRetireRequestV1 *request, GDExtensionQualificationRasterRetireRefV1 *out_retire, GDExtensionQualificationRasterRetireReportV1 *out_report);

/**
 * @name device_session_qualification_raster_unit_retire_observe_v1
 * @since 4.7
 *
 * Original finite bridge-owned depth raster child. Shader, render pipeline and internal framebuffer retain separate actual physical return facts; observed RIDs never authorize admission.
 *
 * @param bridge Synchronous in-process bridge.
 * @param retire Synchronous in-process retire.
 * @param budget Synchronous in-process budget.
 * @param out_report Synchronous in-process out report.
 *
 * @return Original native Error/reason observation. A possible effect requires a retained original child/ticket, never an unowned bare error.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationRasterUnitRetireObserveV1)(GDExtensionQualificationBridgeRefV1 bridge, GDExtensionQualificationRasterRetireRefV1 retire, const GDExtensionQualificationBudgetV1 *budget, GDExtensionQualificationRasterRetireReportV1 *out_report);

/**
 * @name device_session_qualification_raster_unit_retire_release_v1
 * @since 4.7
 *
 * Original finite bridge-owned depth raster child. Shader, render pipeline and internal framebuffer retain separate actual physical return facts; observed RIDs never authorize admission.
 *
 * @param bridge Synchronous in-process bridge.
 * @param retire Synchronous in-process retire.
 *
 * @return Original native Error/reason observation. A possible effect requires a retained original child/ticket, never an unowned bare error.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationRasterUnitRetireReleaseV1)(GDExtensionQualificationBridgeRefV1 bridge, GDExtensionQualificationRasterRetireRefV1 retire);

/**
 * @name device_session_qualification_record_prepared_v4
 * @since 4.7
 *
 * Atomically records the original same-ticket depth producer before compute/sample/readback. Original operation range authenticates provenance, independently of native fact/draw capacities; cleanup end never publishes produced content.
 *
 * @param bridge Synchronous in-process bridge.
 * @param ticket Synchronous in-process ticket.
 * @param request Synchronous in-process request.
 * @param draws Copied or synchronously borrowed within the original admitted finite payload.
 * @param draw_count Copied or synchronously borrowed within the original admitted finite payload.
 * @param push_bytes Copied or synchronously borrowed within the original admitted finite payload.
 * @param push_byte_count Copied or synchronously borrowed within the original admitted finite payload.
 * @param out_report Synchronous in-process out report.
 *
 * @return Original native Error/reason observation. A possible effect requires a retained original child/ticket, never an unowned bare error.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationRecordPreparedV4)(GDExtensionQualificationBridgeRefV1 bridge, GDExtensionQualificationTicketRefV1 ticket, const GDExtensionQualificationRecordV4 *request, const GDExtensionQualificationDepthDrawV1 *draws, uint32_t draw_count, const void *push_bytes, uint64_t push_byte_count, GDExtensionQualificationReportV3 *out_report);

/**
 * @name device_session_qualification_observe_v3
 * @since 4.7
 *
 * Observes the same original ticket, depth report and original fact ledger. No separate completion or caller-supplied success state.
 *
 * @param bridge Synchronous in-process bridge.
 * @param ticket Synchronous in-process ticket.
 * @param budget Synchronous in-process budget.
 * @param out_report Synchronous in-process out report.
 *
 * @return Original native Error/reason observation. A possible effect requires a retained original child/ticket, never an unowned bare error.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationObserveV3)(GDExtensionQualificationBridgeRefV1 bridge, GDExtensionQualificationTicketRefV1 ticket, const GDExtensionQualificationBudgetV1 *budget, GDExtensionQualificationReportV3 *out_report);

/**
 * @name device_session_production_reserve_v2
 * @since 4.7
 *
 * Checked Core V2 physical action-ordinal transport. Original C04/C06/R01 source mapping, logical requirement, once/window and publication receipt authority remain with their original owners.
 *
 * V2 semantic entry points accept only requests reserved with the V2 wire contract; V1 semantic entry points accept only V1 requests. Mismatches reject before mutation or immutable control return consumption. Original identity, resource, receiver, schedule, ACK and cleanup ownership remain shared as explicitly specified.
 * Inputs are synchronous aligned nonaliasing POD spans. New header-bearing V2 DTOs use abi_version 2 and exact containing sizeof; unchanged OperationV1, CapacityV1 and ServiceBudgetV1 retain version 1. Headerless row array stride is selected by this typed entry point.
 * OperationV1 preserves its original nonempty logical range. action_count may be zero and physical resources may be absent when there is no physical work. Every supplied action ordinal equals its array index; original canonicals are in range and nondecreasing. Core validates physical mechanics, not source-derived recipe coverage or logical zero-slice authority.
 *
 * @param attachment Synchronous checked attachment.
 * @param operation Synchronous checked operation.
 * @param capacity Synchronous checked capacity.
 * @param actions Synchronous checked actions.
 * @param action_count Synchronous checked action count.
 * @param resources Synchronous checked resources.
 * @param resource_count Synchronous checked resource count.
 * @param uses Synchronous checked uses.
 * @param use_count Synchronous checked use count.
 * @param readbacks Synchronous checked readbacks.
 * @param readback_count Synchronous checked readback count.
 * @param out_request Synchronous checked out request.
 *
 * @return Original checked native result. Returned API calls and Core physical contexts do not issue original Runtime logical permissions.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionReserveV2)(GDExtensionProductionAttachmentRefV1 attachment, const GDExtensionProductionOperationV1 *operation, const GDExtensionProductionCapacityV1 *capacity, const GDExtensionProductionActionV2 *actions, uint32_t action_count, const GDExtensionProductionResourceV1 *resources, uint32_t resource_count, const GDExtensionProductionUseV1 *uses, uint32_t use_count, const GDExtensionProductionReadbackV2 *readbacks, uint32_t readback_count, GDExtensionProductionRequestRefV1 *out_request);

/**
 * @name device_session_production_reserve_readback_sources_v1
 * @since 4.7
 *
 * Reserves the original production request with explicit writer/incoming source rows. Actions and all prepare/append/observe/facts/release controls retain the V2 dialect; this source-family V1 does not select a V1 request.
 *
 * Inputs are synchronous aligned nonaliasing POD spans. Caller supplies at least readback_count readable source rows of the exact generated type; allocation extent is not inferred. Inactive payloads, unknown tags, identities, ranges and capacity are checked before publishing a request.
 * The output must initially be zero and remains unchanged on failure. Existing V1/V2 reserve entries and their real-writer contracts are unchanged. Incoming is only opaque physical copy under the original retained version/lease and ordered-source prerequisites, not logical publication or completion authority.
 *
 * @param attachment Synchronous checked attachment.
 * @param operation Synchronous checked operation.
 * @param capacity Synchronous checked capacity.
 * @param actions Synchronous checked actions.
 * @param action_count Synchronous checked action count.
 * @param resources Synchronous checked resources.
 * @param resource_count Synchronous checked resource count.
 * @param uses Synchronous checked uses.
 * @param use_count Synchronous checked use count.
 * @param readbacks Synchronous checked readbacks.
 * @param readback_count Synchronous checked readback count.
 * @param out_request Synchronous checked out request.
 *
 * @return Original checked native result. Returned API calls and Core physical contexts do not issue original Runtime logical permissions.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionReserveReadbackSourcesV1)(GDExtensionProductionAttachmentRefV1 attachment, const GDExtensionProductionOperationV1 *operation, const GDExtensionProductionCapacityV1 *capacity, const GDExtensionProductionActionV2 *actions, uint32_t action_count, const GDExtensionProductionResourceV1 *resources, uint32_t resource_count, const GDExtensionProductionUseV1 *uses, uint32_t use_count, const GDExtensionProductionReadbackSourceV1 *readbacks, uint32_t readback_count, GDExtensionProductionRequestRefV1 *out_request);

/**
 * @name device_session_production_prepare_dispatch_v2
 * @since 4.7
 *
 * Checked Core V2 physical action-ordinal transport. Original C04/C06/R01 source mapping, logical requirement, once/window and publication receipt authority remain with their original owners.
 *
 * V2 semantic entry points accept only requests reserved with the V2 wire contract; V1 semantic entry points accept only V1 requests. Mismatches reject before mutation or immutable control return consumption. Original identity, resource, receiver, schedule, ACK and cleanup ownership remain shared as explicitly specified.
 * Inputs are synchronous aligned nonaliasing POD spans. New header-bearing V2 DTOs use abi_version 2 and exact containing sizeof; unchanged OperationV1, CapacityV1 and ServiceBudgetV1 retain version 1. Headerless row array stride is selected by this typed entry point.
 * The payload action ordinal, original canonical and source recipe substep must all match the same frozen row. source_recipe_reserved must be zero. Exact retries retain the original immutable payload and bounds.
 *
 * @param request Synchronous checked request.
 * @param dispatch Synchronous checked dispatch.
 * @param push Synchronous checked push.
 * @param push_bytes Synchronous checked push bytes.
 *
 * @return Original checked native result. Returned API calls and Core physical contexts do not issue original Runtime logical permissions.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionPrepareDispatchV2)(GDExtensionProductionRequestRefV1 request, const GDExtensionProductionDispatchV2 *dispatch, const void *push, uint32_t push_bytes);

/**
 * @name device_session_production_prepare_copy_v2
 * @since 4.7
 *
 * Checked Core V2 physical action-ordinal transport. Original C04/C06/R01 source mapping, logical requirement, once/window and publication receipt authority remain with their original owners.
 *
 * V2 semantic entry points accept only requests reserved with the V2 wire contract; V1 semantic entry points accept only V1 requests. Mismatches reject before mutation or immutable control return consumption. Original identity, resource, receiver, schedule, ACK and cleanup ownership remain shared as explicitly specified.
 * Inputs are synchronous aligned nonaliasing POD spans. New header-bearing V2 DTOs use abi_version 2 and exact containing sizeof; unchanged OperationV1, CapacityV1 and ServiceBudgetV1 retain version 1. Headerless row array stride is selected by this typed entry point.
 * The payload action ordinal, original canonical and source recipe substep must all match the same frozen row. source_recipe_reserved must be zero. Exact retries retain the original immutable payload and bounds.
 *
 * @param request Synchronous checked request.
 * @param copy Synchronous checked copy.
 *
 * @return Original checked native result. Returned API calls and Core physical contexts do not issue original Runtime logical permissions.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionPrepareCopyV2)(GDExtensionProductionRequestRefV1 request, const GDExtensionProductionCopyV2 *copy);

/**
 * @name device_session_production_prepare_upload_v2
 * @since 4.7
 *
 * Checked Core V2 physical action-ordinal transport. Original C04/C06/R01 source mapping, logical requirement, once/window and publication receipt authority remain with their original owners.
 *
 * V2 semantic entry points accept only requests reserved with the V2 wire contract; V1 semantic entry points accept only V1 requests. Mismatches reject before mutation or immutable control return consumption. Original identity, resource, receiver, schedule, ACK and cleanup ownership remain shared as explicitly specified.
 * Inputs are synchronous aligned nonaliasing POD spans. New header-bearing V2 DTOs use abi_version 2 and exact containing sizeof; unchanged OperationV1, CapacityV1 and ServiceBudgetV1 retain version 1. Headerless row array stride is selected by this typed entry point.
 * The payload action ordinal, original canonical and source recipe substep must all match the same frozen row. source_recipe_reserved must be zero. Exact retries retain the original immutable payload and bounds.
 *
 * @param request Synchronous checked request.
 * @param upload Synchronous checked upload.
 * @param payload Synchronous checked payload.
 * @param payload_bytes Synchronous checked payload bytes.
 *
 * @return Original checked native result. Returned API calls and Core physical contexts do not issue original Runtime logical permissions.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionPrepareUploadV2)(GDExtensionProductionRequestRefV1 request, const GDExtensionProductionUploadV2 *upload, const void *payload, uint64_t payload_bytes);

/**
 * @name device_session_production_prepare_readback_v2
 * @since 4.7
 *
 * Checked Core V2 physical action-ordinal transport. Original C04/C06/R01 source mapping, logical requirement, once/window and publication receipt authority remain with their original owners.
 *
 * V2 semantic entry points accept only requests reserved with the V2 wire contract; V1 semantic entry points accept only V1 requests. Mismatches reject before mutation or immutable control return consumption. Original identity, resource, receiver, schedule, ACK and cleanup ownership remain shared as explicitly specified.
 * Inputs are synchronous aligned nonaliasing POD spans. New header-bearing V2 DTOs use abi_version 2 and exact containing sizeof; unchanged OperationV1, CapacityV1 and ServiceBudgetV1 retain version 1. Headerless row array stride is selected by this typed entry point.
 * The ordinal must identify the unique READBACK row for readback_slot and match its declared copy_action_ordinal, original read use and exact physical range.
 *
 * @param request Synchronous checked request.
 * @param action_ordinal Physical action ordinal in the same frozen request. It is never a logical canonical lookup key.
 * @param readback_slot Synchronous checked readback slot.
 *
 * @return Original checked native result. Returned API calls and Core physical contexts do not issue original Runtime logical permissions.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionPrepareReadbackV2)(GDExtensionProductionRequestRefV1 request, uint64_t action_ordinal, uint32_t readback_slot);

/**
 * @name device_session_production_read_revision_v1
 * @since 4.7
 *
 * Reads the exact current revision of an original production request during its executing physical RECORD callback. This versioned read-only query accepts either original V1 or V2 wire request under the same retained physical context.
 *
 * The physical context must be the current protected callback frame, on its original rendering thread, with the exact request and EXECUTING RECORD control. A context from another request, an expired frame or an external owner-thread call is rejected; public request identity alone cannot issue this borrow.
 * No state, service budget, queue, control result, append frontier, native observation or revision is changed. The returned revision is an optimistic append precondition, not permission to append or evidence of completion. The original owner observe entry points retain their thread and control semantics.
 * out_revision must be non-null and naturally aligned. On any failure its storage is left untouched. On success it receives the exact revision under the original Registry lock; callers must requery after synchronous operations that can change the revision.
 *
 * @param context The current original protected physical callback context.
 * @param request The exact original request of that physical callback.
 * @param out_revision Naturally aligned output; written only after successful original frame validation.
 *
 * @return Original checked result. Failure leaves out_revision untouched and does not consume a control.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionReadRevisionV1)(GDExtensionProductionPhysicalContextRefV1 context, GDExtensionProductionRequestRefV1 request, uint64_t *out_revision);

/**
 * @name device_session_production_append_v2
 * @since 4.7
 *
 * Checked Core V2 physical action-ordinal transport. Original C04/C06/R01 source mapping, logical requirement, once/window and publication receipt authority remain with their original owners.
 *
 * V2 semantic entry points accept only requests reserved with the V2 wire contract; V1 semantic entry points accept only V1 requests. Mismatches reject before mutation or immutable control return consumption. Original identity, resource, receiver, schedule, ACK and cleanup ownership remain shared as explicitly specified.
 * Inputs are synchronous aligned nonaliasing POD spans. New header-bearing V2 DTOs use abi_version 2 and exact containing sizeof; unchanged OperationV1, CapacityV1 and ServiceBudgetV1 retain version 1. Headerless row array stride is selected by this typed entry point.
 * action_ordinal must equal the original request physical next-action cursor at the supplied revision. Returned canonical and recipe substep come from that frozen row. A partial upload prefix may continue only its exact physical parent row.
 *
 * @param physical_context Synchronous checked physical context.
 * @param request Synchronous checked request.
 * @param action_ordinal Physical action ordinal in the same frozen request. It is never a logical canonical lookup key.
 * @param revision Synchronous checked revision.
 * @param out_append Synchronous checked out append.
 *
 * @return Original checked native result. Returned API calls and Core physical contexts do not issue original Runtime logical permissions.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionAppendV2)(GDExtensionProductionPhysicalContextRefV1 physical_context, GDExtensionProductionRequestRefV1 request, uint64_t action_ordinal, uint64_t revision, GDExtensionProductionAppendV2 *out_append);

/**
 * @name device_session_production_close_segment_v2
 * @since 4.7
 *
 * Checked Core V2 physical action-ordinal transport. Original C04/C06/R01 source mapping, logical requirement, once/window and publication receipt authority remain with their original owners.
 *
 * V2 semantic entry points accept only requests reserved with the V2 wire contract; V1 semantic entry points accept only V1 requests. Mismatches reject before mutation or immutable control return consumption. Original identity, resource, receiver, schedule, ACK and cleanup ownership remain shared as explicitly specified.
 * Inputs are synchronous aligned nonaliasing POD spans. New header-bearing V2 DTOs use abi_version 2 and exact containing sizeof; unchanged OperationV1, CapacityV1 and ServiceBudgetV1 retain version 1. Headerless row array stride is selected by this typed entry point.
 * Returned physical action frontiers and evidence do not manufacture original logical completion, zero-slice execution or publication receipts. Existing native Unknown, return, ACK and cleanup budget rules remain unchanged.
 *
 * @param physical_context Synchronous checked physical context.
 * @param request Synchronous checked request.
 * @param out_segment Synchronous checked out segment.
 *
 * @return Original checked native result. Returned API calls and Core physical contexts do not issue original Runtime logical permissions.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionCloseSegmentV2)(GDExtensionProductionPhysicalContextRefV1 physical_context, GDExtensionProductionRequestRefV1 request, GDExtensionProductionSegmentV2 *out_segment);

/**
 * @name device_session_production_record_consume_v2
 * @since 4.7
 *
 * Checked Core V2 physical action-ordinal transport. Original C04/C06/R01 source mapping, logical requirement, once/window and publication receipt authority remain with their original owners.
 *
 * V2 semantic entry points accept only requests reserved with the V2 wire contract; V1 semantic entry points accept only V1 requests. Mismatches reject before mutation or immutable control return consumption. Original identity, resource, receiver, schedule, ACK and cleanup ownership remain shared as explicitly specified.
 * Inputs are synchronous aligned nonaliasing POD spans. New header-bearing V2 DTOs use abi_version 2 and exact containing sizeof; unchanged OperationV1, CapacityV1 and ServiceBudgetV1 retain version 1. Headerless row array stride is selected by this typed entry point.
 * Returned physical action frontiers and evidence do not manufacture original logical completion, zero-slice execution or publication receipts. Existing native Unknown, return, ACK and cleanup budget rules remain unchanged.
 *
 * @param request Synchronous checked request.
 * @param control_serial Synchronous checked control serial.
 * @param out_report Synchronous checked out report.
 *
 * @return Original checked native result. Returned API calls and Core physical contexts do not issue original Runtime logical permissions.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionRecordConsumeV2)(GDExtensionProductionRequestRefV1 request, uint64_t control_serial, GDExtensionProductionRecordReportV2 *out_report);

/**
 * @name device_session_production_observe_v2
 * @since 4.7
 *
 * Checked Core V2 physical action-ordinal transport. Original C04/C06/R01 source mapping, logical requirement, once/window and publication receipt authority remain with their original owners.
 *
 * V2 semantic entry points accept only requests reserved with the V2 wire contract; V1 semantic entry points accept only V1 requests. Mismatches reject before mutation or immutable control return consumption. Original identity, resource, receiver, schedule, ACK and cleanup ownership remain shared as explicitly specified.
 * Inputs are synchronous aligned nonaliasing POD spans. New header-bearing V2 DTOs use abi_version 2 and exact containing sizeof; unchanged OperationV1, CapacityV1 and ServiceBudgetV1 retain version 1. Headerless row array stride is selected by this typed entry point.
 * Returned physical action frontiers and evidence do not manufacture original logical completion, zero-slice execution or publication receipts. Existing native Unknown, return, ACK and cleanup budget rules remain unchanged.
 *
 * @param request Synchronous checked request.
 * @param budget Synchronous checked budget.
 * @param out_report Synchronous checked out report.
 *
 * @return Original checked native result. Returned API calls and Core physical contexts do not issue original Runtime logical permissions.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionObserveV2)(GDExtensionProductionRequestRefV1 request, const GDExtensionProductionServiceBudgetV1 *budget, GDExtensionProductionReportV2 *out_report);

/**
 * @name device_session_production_facts_v2
 * @since 4.7
 *
 * Checked Core V2 physical action-ordinal transport. Original C04/C06/R01 source mapping, logical requirement, once/window and publication receipt authority remain with their original owners.
 *
 * V2 semantic entry points accept only requests reserved with the V2 wire contract; V1 semantic entry points accept only V1 requests. Mismatches reject before mutation or immutable control return consumption. Original identity, resource, receiver, schedule, ACK and cleanup ownership remain shared as explicitly specified.
 * Inputs are synchronous aligned nonaliasing POD spans. New header-bearing V2 DTOs use abi_version 2 and exact containing sizeof; unchanged OperationV1, CapacityV1 and ServiceBudgetV1 retain version 1. Headerless row array stride is selected by this typed entry point.
 * first is a fact-page index across graph and native rows, not an action ordinal. Each V2 fact carries the exact frozen action ordinal, canonical and recipe substep; membership_parent remains a graph-fact index.
 *
 * @param request Synchronous checked request.
 * @param revision Synchronous checked revision.
 * @param first Synchronous checked first.
 * @param out_facts Synchronous checked out facts.
 * @param fact_capacity Synchronous checked fact capacity.
 * @param out_written Synchronous checked out written.
 *
 * @return Original checked native result. Returned API calls and Core physical contexts do not issue original Runtime logical permissions.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionFactsV2)(GDExtensionProductionRequestRefV1 request, uint64_t revision, uint32_t first, GDExtensionProductionFactV2 *out_facts, uint32_t fact_capacity, uint32_t *out_written);

/**
 * @name device_session_production_release_v2
 * @since 4.7
 *
 * Checked Core V2 physical action-ordinal transport. Original C04/C06/R01 source mapping, logical requirement, once/window and publication receipt authority remain with their original owners.
 *
 * V2 semantic entry points accept only requests reserved with the V2 wire contract; V1 semantic entry points accept only V1 requests. Mismatches reject before mutation or immutable control return consumption. Original identity, resource, receiver, schedule, ACK and cleanup ownership remain shared as explicitly specified.
 * Inputs are synchronous aligned nonaliasing POD spans. New header-bearing V2 DTOs use abi_version 2 and exact containing sizeof; unchanged OperationV1, CapacityV1 and ServiceBudgetV1 retain version 1. Headerless row array stride is selected by this typed entry point.
 * Returned physical action frontiers and evidence do not manufacture original logical completion, zero-slice execution or publication receipts. Existing native Unknown, return, ACK and cleanup budget rules remain unchanged.
 *
 * @param request Original accepted request.
 * @param input Explicit physical service slice, copied into the same fixed cell before publication; never a Runtime logical grant.
 * @param out_remaining Original ABI/header/deadline. Pending returns zero available counts while the full slice remains escrowed. Returned reports only the old cell actual remaining; discard unused old balance, never credit a new meter.
 * @param out_report Actual immutable original result/report plus fixed service serial/pending state. Returned never accepts a fresh turn in the same call.
 *
 * @return Original checked native result. Returned API calls and Core physical contexts do not issue original Runtime logical permissions.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionReleaseV2)(GDExtensionProductionRequestRefV1 request, const GDExtensionProductionServiceBudgetV1 *input, GDExtensionProductionServiceBudgetV1 *out_remaining, GDExtensionProductionReportV2 *out_report);

/**
 * @name device_session_output_create_v1
 * @since 4.7
 *
 * Synchronous original native ExternalOutput create body. Symbol availability grants no source, Host, charge, phase or completion authority.
 *
 * All pointer extents must be live and accessible for this call. Nonempty RID spans must be aligned and pairwise disjoint from the input and output. No input pointer is retained. Detail strings have static engine lifetime.
 * Create and seal retain their original source/profile and record-bridge admission checks. Cancel and withdraw use the existing native state transitions. Separately exported acquire and poll preserve exact original request/action custody and do not imply profile availability.
 *
 * @param physical_context Actual synchronous protected RECORD callback context; expires when that callback returns.
 * @param request The original admitted request of that same callback.
 * @param input Aligned readable input, borrowed only until this call returns; its ABI size is sizeof this DTO.
 * @param out_result Aligned writable result, disjoint from input and all input spans. If either DTO pointer or a RID span is null (when nonempty), misaligned, overflowing or overlaps the output, the result is left untouched. Other checked contract failures write a static CONTRACT status.
 */
typedef void (*GDExtensionInterfaceDeviceSessionOutputCreateV1)(GDExtensionProductionPhysicalContextRefV1 physical_context, GDExtensionProductionRequestRefV1 request, const GDExtensionOutputCreateInputV1 *input, GDExtensionOutputCreateResultV1 *out_result);

/**
 * @name device_session_output_create_v2
 * @since 4.7
 *
 * Synchronous native ExternalOutput creation using an exact original accepted OUTPUT_METADATA resource row. The outer ABI version is 2 and struct_size is sizeof(GDExtensionOutputCreateInputV2); reserved fields are zero. The nested config retains its unchanged V1 ABI and layout.
 *
 * The metadata resource slot belongs to the same original request, has CREATED_BY_OWNER ownership and sufficient declared physical_bytes, and is consumed once. Native allocation and generation custody do not attest Runtime logical source/account admission or a U08 payment.
 * All pointer extents must be live and accessible for this call. Nonempty nested RID spans must be aligned and pairwise disjoint from the entire V2 input and output. No input pointer is retained. Detail strings have static engine lifetime.
 * Currently only SceneOnly creation is supported; render profiles retain their native preparation refusal. A successful generation retains its metadata resource until actual particles_free after Released; generation terminal alone does not return that resource. V1 creation retains its original admission refusal.
 *
 * @param physical_context Actual synchronous protected RECORD callback context; expires when that callback returns.
 * @param request The original accepted request owning the metadata resource row and generation custody.
 * @param input Borrowed nested configuration and original metadata resource slot. UINT32_MAX is not a valid slot.
 * @param out_result Unchanged V1 result. If either DTO extent or a nonempty RID span is null, misaligned, overflowing or overlaps the output, the result is untouched. Other checked contract failures write CONTRACT with no new RID or generation.
 */
typedef void (*GDExtensionInterfaceDeviceSessionOutputCreateV2)(GDExtensionProductionPhysicalContextRefV1 physical_context, GDExtensionProductionRequestRefV1 request, const GDExtensionOutputCreateInputV2 *input, GDExtensionOutputCreateResultV1 *out_result);

/**
 * @name device_session_output_seal_v1
 * @since 4.7
 *
 * Synchronous original native ExternalOutput seal body. Symbol availability grants no source, Host, charge, phase or completion authority.
 *
 * All pointer extents must be live and accessible for this call. Nonempty RID spans must be aligned and pairwise disjoint from the input and output. No input pointer is retained. Detail strings have static engine lifetime.
 * Create and seal retain their original source/profile and record-bridge admission checks. Cancel and withdraw use the existing native state transitions. Separately exported acquire and poll preserve exact original request/action custody and do not imply profile availability.
 *
 * @param physical_context Actual synchronous protected RECORD callback context; expires when that callback returns.
 * @param request The original admitted request of that same callback.
 * @param input Aligned readable input, borrowed only until this call returns; its ABI size is sizeof this DTO.
 * @param out_result Aligned writable result, disjoint from input and all input spans. If either DTO pointer or a RID span is null (when nonempty), misaligned, overflowing or overlaps the output, the result is left untouched. Other checked contract failures write a static CONTRACT status.
 */
typedef void (*GDExtensionInterfaceDeviceSessionOutputSealV1)(GDExtensionProductionPhysicalContextRefV1 physical_context, GDExtensionProductionRequestRefV1 request, const GDExtensionOutputSealInputV1 *input, GDExtensionOutputSealResultV1 *out_result);

/**
 * @name device_session_output_cancel_v1
 * @since 4.7
 *
 * Synchronous original native ExternalOutput cancel body. Symbol availability grants no source, Host, charge, phase or completion authority.
 *
 * All pointer extents must be live and accessible for this call. Nonempty RID spans must be aligned and pairwise disjoint from the input and output. No input pointer is retained. Detail strings have static engine lifetime.
 * Create and seal retain their original source/profile and record-bridge admission checks. Cancel and withdraw use the existing native state transitions. Separately exported acquire and poll preserve exact original request/action custody and do not imply profile availability.
 *
 * @param physical_context Actual synchronous protected RECORD callback context; expires when that callback returns.
 * @param request The original admitted request of that same callback.
 * @param input Aligned readable input, borrowed only until this call returns; its ABI size is sizeof this DTO.
 *
 * @return Original static-detail status, or CONTRACT for an invalid input pointer.
 */
typedef GDExtensionOutputStatusV1 (*GDExtensionInterfaceDeviceSessionOutputCancelV1)(GDExtensionProductionPhysicalContextRefV1 physical_context, GDExtensionProductionRequestRefV1 request, const GDExtensionOutputCancelInputV1 *input);

/**
 * @name device_session_output_withdraw_v1
 * @since 4.7
 *
 * Synchronous original native ExternalOutput withdraw body. Symbol availability grants no source, Host, charge, phase or completion authority.
 *
 * All pointer extents must be live and accessible for this call. Nonempty RID spans must be aligned and pairwise disjoint from the input and output. No input pointer is retained. Detail strings have static engine lifetime.
 * Create and seal retain their original source/profile and record-bridge admission checks. Cancel and withdraw use the existing native state transitions. Separately exported acquire and poll preserve exact original request/action custody and do not imply profile availability.
 *
 * @param physical_context Actual synchronous protected RECORD callback context; expires when that callback returns.
 * @param request The original admitted request of that same callback.
 * @param input Aligned readable input, borrowed only until this call returns; its ABI size is sizeof this DTO.
 *
 * @return Original static-detail status, or CONTRACT for an invalid input pointer.
 */
typedef GDExtensionOutputStatusV1 (*GDExtensionInterfaceDeviceSessionOutputWithdrawV1)(GDExtensionProductionPhysicalContextRefV1 physical_context, GDExtensionProductionRequestRefV1 request, const GDExtensionOutputGenerationInputV1 *input);

/**
 * @name device_session_production_raster_begin_v1
 * @since 4.7
 *
 * Begin the reserved depth-clear raster frame in its original Production physical callback. Captures checked begin and clear facts under the same request/action; target content publication and drawing are not part of this version.
 *
 * @param context Synchronous typed input.
 * @param request Synchronous typed input.
 * @param revision Synchronous typed input.
 *
 * @return Checked Production result.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionRasterBeginV1)(GDExtensionProductionPhysicalContextRefV1 context, GDExtensionProductionRequestRefV1 request, uint64_t revision);

/**
 * @name device_session_production_raster_end_v1
 * @since 4.7
 *
 * Close the reserved raster frame and capture its checked end. The original callback return also closes an open frame. Failed closure retains possible effects and all physical custody.
 *
 * @param context Synchronous typed input.
 * @param request Synchronous typed input.
 *
 * @return Checked Production result.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionRasterEndV1)(GDExtensionProductionPhysicalContextRefV1 context, GDExtensionProductionRequestRefV1 request);

/**
 * @name device_session_production_control_receipt_v1
 * @since 4.8
 *
 * Query the authenticated original request control cell without admission, scheduling, consuming or changing its service budget. Query immediately after OBSERVE V2/V3 and before another control. expected_control_serial zero discovers the current cell; nonzero requires exact equality and rejects overwritten history. Failure clears the output and grants no disposition. Readability during quiesce follows original production maintenance admission.
 *
 * @param request Original request owner reference.
 * @param expected_control_serial Zero discovers current serial; otherwise require exact current serial.
 * @param out_receipt Original cell snapshot; cleared on failure.
 *
 * @return Query success or authentication, stale serial or parameter failure; separate from the projected control result.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionControlReceiptV1)(GDExtensionProductionRequestRefV1 request, uint64_t expected_control_serial, GDExtensionProductionControlReceiptV1 *out_receipt);

/**
 * @name device_session_production_record_history_v1
 * @since 4.7
 *
 * Read the retained history of the exact current RECORD control, including after owner consume. A failed query is never a no-effect certificate. This query performs no scheduling, polling or consumption.
 *
 * @param request Synchronous typed history input or output.
 * @param control_serial Synchronous typed history input or output.
 * @param out_history Synchronous typed history input or output.
 *
 * @return Query result; output is valid only on success.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionRecordHistoryV1)(GDExtensionProductionRequestRefV1 request, uint64_t control_serial, GDExtensionProductionRecordHistoryV1 *out_history);

/**
 * @name device_session_production_raster_prepare_v1
 * @since 4.7
 *
 * Prepare original request-owned raster resources through the original fixed FIFO, or read their retained preparation report. Exact retry is idempotent. Only phase 5 and successful returned creation establish preparation; read-only report does not consume or schedule.
 *
 * @param request Typed preparation input or output.
 * @param prepare Typed preparation input or output.
 * @param shader_binary Typed preparation input or output.
 * @param bytes Typed preparation input or output.
 * @param out_report Typed preparation input or output.
 *
 * @return Operation result; retained physical prefixes remain owned on failure.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionRasterPrepareV1)(GDExtensionProductionRequestRefV1 request, const GDExtensionProductionRasterPrepareV1 *prepare, const void *shader_binary, uint64_t bytes, GDExtensionProductionRasterPrepareReportV1 *out_report);

/**
 * @name device_session_production_raster_prepare_report_v1
 * @since 4.7
 *
 * Prepare original request-owned raster resources through the original fixed FIFO, or read their retained preparation report. Exact retry is idempotent. Only phase 5 and successful returned creation establish preparation; read-only report does not consume or schedule.
 *
 * @param request Typed preparation input or output.
 * @param out_report Typed preparation input or output.
 *
 * @return Operation result; retained physical prefixes remain owned on failure.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionRasterPrepareReportV1)(GDExtensionProductionRequestRefV1 request, GDExtensionProductionRasterPrepareReportV1 *out_report);

/**
 * @name device_session_production_prepare_output_v1
 * @since 4.7
 *
 * Prepare the exact original native-output action before freeze. Copies only validated immutable data into prequoted original arena storage; availability is not source or account admission.
 *
 * @param request Original accepted request owning the frozen action and its finite resource/use arena.
 * @param prepare Aligned readable preparation DTO, borrowed synchronously and copied only after original admission checks.
 *
 * @return Original preparation status; success does not grant native acquisition or GPU completion.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionPrepareOutputV1)(GDExtensionProductionRequestRefV1 request, const GDExtensionProductionOutputPrepareV1 *prepare);

/**
 * @name device_session_output_acquire_v1
 * @since 4.7
 *
 * Acquire only the exact original accepted/prepared native output action in its protected RECORD callback. Rechecks original action/frontier/source, finite role capacities and actual owner before native allocation.
 *
 * All pointers designate live accessible aligned DTOs for this synchronous call. Invalid, overflowing or overlapping input/output extents leave the output untouched. Detail strings have static engine lifetime.
 *
 * @param physical_context Actual protected RECORD callback context for this same original request.
 * @param request Original accepted request owning the frozen action and its finite resource/use arena.
 * @param action_ordinal Exact native accepted action ordinal; never a compiler Output SOURCE ordinal.
 * @param input Aligned readable input DTO whose complete extent is disjoint from the output.
 * @param out_result Aligned writable result, left untouched when either DTO extent is invalid or overlaps the other.
 */
typedef void (*GDExtensionInterfaceDeviceSessionOutputAcquireV1)(GDExtensionProductionPhysicalContextRefV1 physical_context, GDExtensionProductionRequestRefV1 request, uint64_t action_ordinal, const GDExtensionOutputAcquireInputV1 *input, GDExtensionOutputAcquireResultV1 *out_result);

/**
 * @name device_session_output_poll_v1
 * @since 4.7
 *
 * Observe the same original output generation and request association without scheduling or appending. Original metadata owner must remain alive through this call.
 *
 * All pointers designate live accessible aligned DTOs. Invalid, overflowing or overlapping extents leave the output untouched. Detail strings have static engine lifetime.
 *
 * @param physical_context Actual protected RECORD callback context for this same original request.
 * @param request Original accepted request owning the frozen action and its finite resource/use arena.
 * @param input Aligned readable input DTO whose complete extent is disjoint from the output.
 * @param out_result Aligned writable result, left untouched when either DTO extent is invalid or overlaps the other.
 */
typedef void (*GDExtensionInterfaceDeviceSessionOutputPollV1)(GDExtensionProductionPhysicalContextRefV1 physical_context, GDExtensionProductionRequestRefV1 request, const GDExtensionOutputGenerationInputV1 *input, GDExtensionOutputPollResultV1 *out_result);

/**
 * @name device_session_output_create_v3
 * @since 4.7
 *
 * Exact version-3 Create transport. Recomputes versioned native metadata requirements and requires exact accepted metadata bytes before allocation. V2 is unchanged. Query grants no account, source, selection, or physical custody. Currently SceneOnly with all consumer limits zero; render profiles remain unavailable.
 *
 * @param physical_context Actual synchronous protected RECORD callback context; expires when that callback returns.
 * @param request The original accepted request owning the metadata resource row and generation custody.
 * @param input Exact ABI3 configuration, complete native requirements projection including explicit finite limits, and original metadata resource slot. Create recomputes requirements before allocation.
 * @param out_result Unchanged V1 result. If either DTO extent or a nonempty RID span is null, misaligned, overflowing or overlaps the output, the result is untouched. Other checked contract failures write CONTRACT with no new RID or generation.
 */
typedef void (*GDExtensionInterfaceDeviceSessionOutputCreateV3)(GDExtensionProductionPhysicalContextRefV1 physical_context, GDExtensionProductionRequestRefV1 request, const GDExtensionOutputCreateInputV3 *input, GDExtensionOutputCreateResultV1 *out_result);

/**
 * @name device_session_output_metadata_query_v1
 * @since 4.7
 *
 * Read-only native requirements on the actual main render thread. No production context is needed and none is issued. No native factory, selection lease, account credit or stable quote handle is created. Caller must provide disjoint readable input spans and writable output. Create recomputes all requirements. Currently SceneOnly with explicit zero consumer limits only.
 *
 * @param input Borrowed exact V1 configuration and explicit finite consumer limits. Query performs no allocation, pin, or selection retention.
 * @param out_result Versioned native requirements and status only, with no RID, generation, resource slot or authorization. Invalid or overlapping pointer extents leave output untouched; checked contract errors return status.
 */
typedef void (*GDExtensionInterfaceDeviceSessionOutputMetadataQueryV1)(const GDExtensionOutputMetadataQueryInputV1 *input, GDExtensionOutputMetadataQueryResultV1 *out_result);

/**
 * @name device_session_scene_operation_query_v1
 * @since 4.7
 *
 * Query an original accepted Scene action fact without issuing a phase permit.
 *
 * @param request Original live production request owned by this device attachment.
 * @param action_ordinal Original action index within the retained request.
 * @param key Immutable original operation, cursor and schedule association; caller bytes alone grant no authority.
 * @param outcome Output carrying the original append fact or a refusal; no new record permission is issued by a query.
 *
 * @return Structured native result. Busy preserves original in-flight custody for a later bounded service turn.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionSceneOperationQueryV1)(GDExtensionProductionRequestRefV1 request, uint64_t action_ordinal, const GDExtensionSceneOperationKeyV1 *key, GDExtensionSceneReadOutcomeV1 *outcome);

/**
 * @name device_session_scene_operation_bind_v1
 * @since 4.7
 *
 * Validate the original Scene operation transport. This entry returns unavailable until the original E09 operation/cursor exporter is joined.
 *
 * @param request Original live production request owned by this device attachment.
 * @param action_ordinal Original action index within the retained request.
 * @param key Immutable original operation, cursor and schedule association; caller bytes alone grant no authority.
 *
 * @return Structured native result. Busy preserves original in-flight custody for a later bounded service turn.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionSceneOperationBindV1)(GDExtensionProductionRequestRefV1 request, uint64_t action_ordinal, const GDExtensionSceneOperationKeyV1 *key);

/**
 * @name device_session_scene_phase_capability_v1
 * @since 4.7
 *
 * Obtain a locator for the original attachment device owner.
 *
 * @param attachment Original accepted production attachment for the live device and library generation.
 * @param out_capability Output locator for that original device owner; it must be validated when used.
 *
 * @return Structured native result. Busy preserves original in-flight custody for a later bounded service turn.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionScenePhaseCapabilityV1)(GDExtensionProductionAttachmentRefV1 attachment, GDExtensionSceneOpaque32V1 *out_capability);

/**
 * @name device_session_camera_ready_relay_v1
 * @since 4.7
 *
 * Install a typed camera-ready receiver on an existing adopted extension Callable and retain its independent original normal protected code holder. This installs callback custody only; it does not issue U05 view or RECORD authority.
 *
 * @param callable Original adopted extension Callable retaining its existing holder and userdata.
 * @param receiver Typed camera-ready leaf installed once on that original Callable.
 * @param out_relay Output opaque locator for the original normal-entry relay.
 *
 * @return Structured original normal-entry holder installation result.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionCameraReadyRelayV1)(GDExtensionConstTypePtr callable, GDExtensionProductionCameraReadyReceiverV1 receiver, GDExtensionNormalRelayRefV1 *out_relay);

/**
 * @name device_session_camera_ready_bind_relay_v1
 * @since 4.7
 *
 * Bind the exact already installed original typed camera-ready relay to the same original production attachment. This associates normal protected code-holder custody only; it creates no U05 or native view grant.
 *
 * @param attachment Original production attachment.
 * @param relay Exact original installed typed camera-ready normal relay.
 *
 * @return Original attachment and typed relay association result.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionCameraReadyBindRelayV1)(GDExtensionProductionAttachmentRefV1 attachment, GDExtensionNormalRelayRefV1 relay);

/**
 * @name device_session_camera_ready_bind_viewport_v1
 * @since 4.7
 *
 * Bind the exact already installed original typed camera-ready relay to the selected registered renderer viewport. The viewport RID is only the owner-selected lookup; source authority remains the renderer-owned CameraCapture Ref delivered after draw completion.
 *
 * @param attachment Original production attachment.
 * @param relay Exact original installed typed camera-ready normal relay.
 * @param viewport_rid Exact registered renderer viewport RID selected by the original owner.
 *
 * @return Original attachment, relay, and renderer subscription result.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionCameraReadyBindViewportV1)(GDExtensionProductionAttachmentRefV1 attachment, GDExtensionNormalRelayRefV1 relay, uint64_t viewport_rid);

/**
 * @name device_session_camera_ready_bind_request_v1
 * @since 4.8
 *
 * Bind one already reserved original production request/action to the exact selected renderer viewport and typed camera-ready relay. The request/action is the authority; the viewport RID only selects the renderer owner. The observer stores the association in the paid action cell and forwards the exact retained CameraCapture sample to the existing attachment relay.
 *
 * @param request Original reserved production request owned by the selected attachment.
 * @param action_ordinal Native-output action ordinal inside the original request.
 * @param relay Exact installed typed camera-ready relay already bound to the request attachment.
 * @param viewport_rid Exact registered renderer viewport RID selected by the owner; it grants no authority by itself.
 *
 * @return Structured association result. Busy or unavailable leaves the original request/action and relay retryable.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionCameraReadyBindRequestV1)(GDExtensionProductionRequestRefV1 request, uint64_t action_ordinal, GDExtensionNormalRelayRefV1 relay, uint64_t viewport_rid);

/**
 * @name device_session_camera_ready_close_v1
 * @since 4.7
 *
 * Close the exact original camera-ready relay using its retained wire on the original attachment owner/destruction thread, including an installed relay whose bind failed. Retry accepts the same cancelled wire without requiring live encode. It never installs a Scene registration or changes the RECORD receiver.
 *
 * @param attachment Original attachment owning the bound camera-ready relay.
 * @param relay Exact original camera-ready wire, including after cancellation. May be unbound only when no other original attachment owns it.
 * @param out_close Original entry, active delivery, relay cell and actual holder close observations.
 *
 * @return Original relay cancellation and retirement result; successful close is cached for retries.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionCameraReadyCloseV1)(GDExtensionProductionAttachmentRefV1 attachment, GDExtensionNormalRelayRefV1 relay, GDExtensionCameraReadyCloseV1 *out_close);

/**
 * @name device_session_scene_phase_relay_v1
 * @since 4.7
 *
 * Install a typed phase receiver on an existing adopted extension Callable and retain its original normal-entry holder.
 *
 * @param callable Original adopted extension Callable retaining its existing holder and userdata.
 * @param receiver Typed receiver leaf installed once on that original Callable.
 * @param out_relay Output opaque locator for the original normal-entry relay.
 *
 * @return Structured native result. Busy preserves original in-flight custody for a later bounded service turn.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionScenePhaseRelayV1)(GDExtensionConstTypePtr callable, GDExtensionScenePhaseReceiverV1 receiver, GDExtensionNormalRelayRefV1 *out_relay);

/**
 * @name device_session_scene_phase_register_v1
 * @since 4.7
 *
 * Register the original typed relay for the actual SceneBeforeParticles boundary with a bounded operation limit.
 *
 * @param relay Opaque locator resolved against the original live normal-entry relay.
 * @param attachment Original accepted production attachment for the live device and library generation.
 * @param config Exact ABI, original device capability, phase and bounded operation limit.
 * @param out_registration Zero-initialized output for the original attachment registration.
 *
 * @return Structured native result. Busy preserves original in-flight custody for a later bounded service turn.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionScenePhaseRegisterV1)(GDExtensionNormalRelayRefV1 relay, GDExtensionProductionAttachmentRefV1 attachment, const GDExtensionScenePhaseSinkConfigV1 *config, GDExtensionScenePhaseRegistrationRefV1 *out_registration);

/**
 * @name device_session_scene_phase_unregister_v1
 * @since 4.7
 *
 * Close future phase delivery and service original waiting controls and relay holder closure without requiring another draw.
 *
 * @param registration Opaque locator for the original phase registration to close.
 * @param out_close Output of actual registration, delivery and original holder closure observations.
 *
 * @return Structured native result. Busy preserves original in-flight custody for a later bounded service turn.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionScenePhaseUnregisterV1)(GDExtensionScenePhaseRegistrationRefV1 registration, GDExtensionScenePhaseCloseV1 *out_close);

/**
 * @name device_session_qualification_scene_depth_quote_v1
 * @since 4.7
 *
 * Original Scene-depth quote transaction on the retained DeviceExit bridge. Pure admission failure has API error and zero output. Accepted transaction has API OK and same-facet output even when original native quote_error is nonzero. Such failure signs no native quote and must close through original Retire/observe/release ACK. No general QRT error/output rule changes.
 *
 * @param bridge Synchronous copied original transaction bridge. No caller pointer survives.
 * @param request Synchronous copied original transaction request. No caller pointer survives.
 * @param out_quote Synchronous copied original transaction out_quote. No caller pointer survives.
 *
 * @return Original admission/delivery Error/reason; inspect typed creation_error and custody for an accepted native creation attempt.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationSceneDepthQuoteV1)(GDExtensionQualificationBridgeRefV1 bridge, const GDExtensionStorageSceneDepthQuoteRequestV1 *request, GDExtensionStorageSceneDepthQuoteV1 *out_quote);

/**
 * @name device_session_qualification_scene_depth_create_v1
 * @since 4.7
 *
 * Typed Scene-depth neighbor on the original DeviceExit bridge, session, CodeHolder and fixed RT request cell. No second registry or K01 authority. API failure has zero output; accepted native failure is delivered in Report.creation_error with original custody.
 *
 * @param bridge Synchronous copied original transaction bridge. No caller pointer survives.
 * @param request Synchronous copied original transaction request. No caller pointer survives.
 * @param out_report Synchronous copied original transaction out_report. No caller pointer survives.
 *
 * @return Original admission/delivery Error/reason; inspect typed creation_error and custody for an accepted native creation attempt.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationSceneDepthCreateV1)(GDExtensionQualificationBridgeRefV1 bridge, const GDExtensionStorageSceneDepthCreateRequestV1 *request, GDExtensionStorageSceneDepthReportV1 *out_report);

/**
 * @name device_session_qualification_scene_depth_retire_v1
 * @since 4.7
 *
 * Typed Scene-depth neighbor on the original DeviceExit bridge, session, CodeHolder and fixed RT request cell. No second registry or K01 authority. API failure has zero output; accepted native failure is delivered in Report.creation_error with original custody.
 *
 * @param bridge Synchronous copied original transaction bridge. No caller pointer survives.
 * @param request Synchronous copied original transaction request. No caller pointer survives.
 * @param out_report Synchronous copied original transaction out_report. No caller pointer survives.
 *
 * @return Original admission/delivery Error/reason; inspect typed creation_error and custody for an accepted native creation attempt.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationSceneDepthRetireV1)(GDExtensionQualificationBridgeRefV1 bridge, const GDExtensionStorageSceneDepthRetireRequestV1 *request, GDExtensionStorageSceneDepthReportV1 *out_report);

/**
 * @name device_session_qualification_scene_depth_observe_v1
 * @since 4.7
 *
 * Typed Scene-depth neighbor on the original DeviceExit bridge, session, CodeHolder and fixed RT request cell. No second registry or K01 authority. API failure has zero output; accepted native failure is delivered in Report.creation_error with original custody.
 *
 * @param bridge Synchronous copied original transaction bridge. No caller pointer survives.
 * @param facet Synchronous copied original transaction facet. No caller pointer survives.
 * @param budget Synchronous copied original transaction budget. No caller pointer survives.
 * @param out_report Synchronous copied original transaction out_report. No caller pointer survives.
 *
 * @return Original admission/delivery Error/reason; inspect typed creation_error and custody for an accepted native creation attempt.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationSceneDepthObserveV1)(GDExtensionQualificationBridgeRefV1 bridge, GDExtensionStorageSceneDepthRefV1 facet, const GDExtensionQualificationBudgetV1 *budget, GDExtensionStorageSceneDepthReportV1 *out_report);

/**
 * @name device_session_qualification_scene_depth_release_v1
 * @since 4.7
 *
 * Typed Scene-depth neighbor on the original DeviceExit bridge, session, CodeHolder and fixed RT request cell. No second registry or K01 authority. API failure has zero output; accepted native failure is delivered in Report.creation_error with original custody.
 *
 * @param bridge Synchronous copied original transaction bridge. No caller pointer survives.
 * @param facet Synchronous copied original transaction facet. No caller pointer survives.
 *
 * @return Original admission/delivery Error/reason; inspect typed creation_error and custody for an accepted native creation attempt.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationSceneDepthReleaseV1)(GDExtensionQualificationBridgeRefV1 bridge, GDExtensionStorageSceneDepthRefV1 facet);

/**
 * @name device_session_production_observe_v3
 * @since 4.7
 *
 * Services the original OBSERVE with unchanged bounded budget for a V2 request. V3 result ABI is 3; nested terminal ABI is 1. A returned generation terminal is projected without consuming it; legacy observe remains unable to consume it. Zero budget may observe a returned terminal without scheduling.
 *
 * @param request Synchronous aligned nonaliasing POD.
 * @param budget Synchronous aligned nonaliasing POD.
 * @param out_result Synchronous aligned nonaliasing POD.
 *
 * @return Original checked result; no logical completion or source acknowledgment is manufactured.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionObserveV3)(GDExtensionProductionRequestRefV1 request, const GDExtensionProductionServiceBudgetV1 *budget, GDExtensionProductionObserveResultV3 *out_result);

/**
 * @name device_session_production_output_return_consume_v1
 * @since 4.7
 *
 * Consumes exactly one original RETURNED OBSERVE generation terminal after comparing registry/request/control/root/particles/generation. Wrong tuples preserve RETURNED; replay rejects. This is native result ownership consumption, not E05/U09 acknowledgment. Output remains untouched on failure.
 *
 * @param request Synchronous aligned nonaliasing POD.
 * @param expected Synchronous aligned nonaliasing POD.
 * @param out_result Synchronous aligned nonaliasing POD.
 *
 * @return Original checked result; no logical completion or source acknowledgment is manufactured.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionOutputReturnConsumeV1)(GDExtensionProductionRequestRefV1 request, const GDExtensionProductionOutputGenerationRefV1 *expected, GDExtensionProductionOutputGenerationResultV1 *out_result);

/**
 * @name device_session_production_request_requirements_v1
 * @since 4.7
 *
 * DRAFT additive ABI: derive finite per-request capacity from synchronous frozen V2 physical rows for dispatch/copy/upload/readback only. Does not reserve arena space or grant logical/source/recording authority.
 *
 * Graph count is a conservative staging bound, not an observed count. Native count comes from the original live driver profile. Reserve and append recheck live state and may fail if it changes.
 * binding_count is the row total of the original frozen future import-set binding spans, not a native verified descriptor certificate; payload_bytes is the exact retained push plus upload byte total. Only sizing-bearing shape is checked here; ownership/access/list/source tags and inactive payload fields still require original reserve validation. Existing reserve/prepare validates the actual closure. Out quote is unchanged on failure. Native output/raster return ERR_UNAVAILABLE.
 *
 * @param ref Synchronous frozen input span or count; out is written only on success.
 * @param actions Synchronous frozen input span or count; out is written only on success.
 * @param action_count Synchronous frozen input span or count; out is written only on success.
 * @param resources Synchronous frozen input span or count; out is written only on success.
 * @param resource_count Synchronous frozen input span or count; out is written only on success.
 * @param uses Synchronous frozen input span or count; out is written only on success.
 * @param use_count Synchronous frozen input span or count; out is written only on success.
 * @param uploads Synchronous frozen input span or count; out is written only on success.
 * @param upload_count Synchronous frozen input span or count; out is written only on success.
 * @param readbacks Synchronous frozen input span or count; out is written only on success.
 * @param readback_count Synchronous frozen input span or count; out is written only on success.
 * @param binding_count Synchronous frozen input span or count; out is written only on success.
 * @param payload_bytes Synchronous frozen input span or count; out is written only on success.
 * @param out Derived per-request quote; unchanged on failure.
 *
 * @return Original Godot Error from checked sizing; no reservation is created.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionRequestRequirementsV1)(GDExtensionProductionAttachmentRefV1 ref, const GDExtensionProductionActionV2 *actions, uint32_t action_count, const GDExtensionProductionResourceV1 *resources, uint32_t resource_count, const GDExtensionProductionUseV1 *uses, uint32_t use_count, const GDExtensionProductionUploadV2 *uploads, uint32_t upload_count, const GDExtensionProductionReadbackSourceV1 *readbacks, uint32_t readback_count, uint32_t binding_count, uint64_t payload_bytes, GDExtensionProductionMetadataQuoteV1 *out);

/**
 * @name device_session_production_request_requirements_v2
 * @since 4.7
 *
 * Additive read-only requirements for original V2 rows with prepared native output generations. Each NATIVE_OUTPUT action must occur exactly once as Acquire, Finalize or Seal in the supplied output rows. Native demand derives from the original generation owner and live driver. No reservation or logical/source/payment authority is created. All outputs remain untouched on failure. V1 remains unchanged.
 *
 * @param ref Synchronous frozen input span or count; out is written only on success.
 * @param actions Synchronous frozen input span or count; out is written only on success.
 * @param action_count Synchronous frozen input span or count; out is written only on success.
 * @param resources Synchronous frozen input span or count; out is written only on success.
 * @param resource_count Synchronous frozen input span or count; out is written only on success.
 * @param uses Synchronous frozen input span or count; out is written only on success.
 * @param use_count Synchronous frozen input span or count; out is written only on success.
 * @param uploads Synchronous frozen input span or count; out is written only on success.
 * @param upload_count Synchronous frozen input span or count; out is written only on success.
 * @param readbacks Synchronous frozen input span or count; out is written only on success.
 * @param readback_count Synchronous frozen input span or count; out is written only on success.
 * @param binding_count Synchronous frozen input span or count; out is written only on success.
 * @param payload_bytes Synchronous frozen input span or count; out is written only on success.
 * @param outputs Synchronous finite demand span; outputs written only on complete success.
 * @param output_count Synchronous finite demand span; outputs written only on complete success.
 * @param demands Synchronous finite demand span; outputs written only on complete success.
 * @param demand_capacity Synchronous finite demand span; outputs written only on complete success.
 * @param out Derived per-request quote; unchanged on failure.
 *
 * @return Original Godot Error from checked sizing; no reservation is created.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionRequestRequirementsV2)(GDExtensionProductionAttachmentRefV1 ref, const GDExtensionProductionActionV2 *actions, uint32_t action_count, const GDExtensionProductionResourceV1 *resources, uint32_t resource_count, const GDExtensionProductionUseV1 *uses, uint32_t use_count, const GDExtensionProductionUploadV2 *uploads, uint32_t upload_count, const GDExtensionProductionReadbackSourceV1 *readbacks, uint32_t readback_count, uint32_t binding_count, uint64_t payload_bytes, const GDExtensionProductionOutputDemandInputV1 *outputs, uint32_t output_count, GDExtensionProductionOutputGuardDemandV1 *demands, uint32_t demand_capacity, GDExtensionProductionMetadataQuoteV1 *out);

/**
 * @name device_session_production_capture_camera_v1
 * @since 4.7
 *
 * Render-thread capture of an already retained original mono camera sample for the prepared action. Never creates or recaptures a source.
 *
 * @param request Original accepted request.
 * @param action_ordinal Original prepared native-output action.
 * @param viewport_rid Original actual viewport RID; not selected from camera identity.
 *
 * @return Original request status; no source READ or rendering grant.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionCaptureCameraV1)(GDExtensionProductionRequestRefV1 request, uint64_t action_ordinal, uint64_t viewport_rid);

/**
 * @name device_session_production_retain_camera_ready_v1
 * @since 4.7
 *
 * Only in the actual native camera-ready scope, retain its existing exact source in the original paid prepared action. This creates no U05 view or RECORD grant.
 *
 * @param request Original accepted request.
 * @param action_ordinal Original prepared native-output action.
 *
 * @return Original paid action retention status; no source READ or rendering grant.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionRetainCameraReadyV1)(GDExtensionProductionRequestRefV1 request, uint64_t action_ordinal);

/**
 * @name device_session_production_read_camera_ready_v1
 * @since 4.7
 *
 * Read the same original paid action immutable camera-ready sample before preparation context upload. No live viewport query, new source, U05 view or RECORD grant.
 *
 * @param request Original accepted request.
 * @param action_ordinal Original prepared native-output action.
 * @param out_sample Aligned writable immutable value result; untouched on failure.
 *
 * @return Original request status; no source READ or rendering grant.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionReadCameraReadyV1)(GDExtensionProductionRequestRefV1 request, uint64_t action_ordinal, GDExtensionProductionCameraSampleV1 *out_sample);

/**
 * @name device_session_production_read_camera_v1
 * @since 4.7
 *
 * Read the same action retained immutable camera values. Performs no live viewport query and creates no source grant.
 *
 * @param request Original accepted request.
 * @param action_ordinal Original prepared native-output action.
 * @param out_sample Aligned writable immutable value result; untouched on failure.
 *
 * @return Original request status; no source READ or rendering grant.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionReadCameraV1)(GDExtensionProductionRequestRefV1 request, uint64_t action_ordinal, GDExtensionProductionCameraSampleV1 *out_sample);

/**
 * @name device_session_production_prepare_host_transform_v1
 * @since 4.7
 *
 * Bind original Host-authenticated immutable affine to the same paid native-output action before input preparation/freeze; does not issue Host authority.
 *
 * @param p_request Original typed request.
 * @param p_action_ordinal Original typed action ordinal.
 * @param p_input Original typed input.
 *
 * @return Original native admission or contract result.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionPrepareHostTransformV1)(GDExtensionProductionRequestRefV1 p_request, uint64_t p_action_ordinal, const GDExtensionProductionHostTransformInputV1 *p_input);

/**
 * @name device_session_production_prepare_output_scene_v1
 * @since 4.7
 *
 * Bind the original trusted E09 Pending physical scene projection to the same paid Acquire action before its first accepted RECORD/freeze.
 *
 * The original E09 caller retains and revalidates its Pending, selected source, R01 reservation and qualified action mapping through native release. Core retains immutable physical values in that original action; it does not authenticate VFX logical source identities. The same values are idempotent before freeze and different values are rejected. Acquire and Finalize read exact committed SceneCull instance/base/scenario/transform facts on RT without flushing scene work.
 *
 * @param request Original live production request retained by E09.
 * @param action_ordinal Original Acquire action ordinal.
 * @param input Original Pending physical values and the already prepared Host F32 affine/token; values alone are not a source grant.
 *
 * @return Structured native refusal; binding performs no native scene query or append.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionPrepareOutputSceneV1)(GDExtensionProductionRequestRefV1 request, uint64_t action_ordinal, const GDExtensionProductionOutputSceneInputV1 *input);

/**
 * @name device_session_production_read_host_transform_v1
 * @since 4.7
 *
 * Read the original immutable action Host affine/inverse; failure leaves output untouched.
 *
 * @param p_request Original typed request.
 * @param p_action_ordinal Original typed action ordinal.
 * @param r_transform Original typed transform.
 *
 * @return Original native admission or contract result.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionReadHostTransformV1)(GDExtensionProductionRequestRefV1 p_request, uint64_t p_action_ordinal, GDExtensionProductionHostTransformV1 *r_transform);

/**
 * @name device_session_production_prepare_output_plan_v1
 * @since 4.7
 *
 * Freeze original Acquire/Finalize/Seal row correlations, exact semantic READ bindings and admitted finite guard work without replacing source or shader ownership.
 *
 * @param p_request Original typed request.
 * @param p_plan Original typed plan.
 *
 * @return Original native admission or contract result.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionPrepareOutputPlanV1)(GDExtensionProductionRequestRefV1 p_request, const GDExtensionProductionOutputPlanV1 *p_plan);

/**
 * @name device_session_production_finalize_output_v1
 * @since 4.7
 *
 * Execute actual original FinalizeGuardCopy native row and return its exact JD02 association; no synthetic success from static facts.
 *
 * @param p_context Original typed context.
 * @param p_request Original typed request.
 * @param p_action_ordinal Original typed action ordinal.
 * @param r_result Original typed result.
 */
typedef void (*GDExtensionInterfaceDeviceSessionProductionFinalizeOutputV1)(GDExtensionProductionPhysicalContextRefV1 p_context, GDExtensionProductionRequestRefV1 p_request, uint64_t p_action_ordinal, GDExtensionProductionOutputFinalizeResultV1 *r_result);

/**
 * @name device_exit_register_prepare_v1
 * @since 4.7
 *
 * Register once in the same original guard bootstrap after successful V1 guard registration. No V1 layout change. Callback enters ordinary CallbackScope on original owner thread before quiescing; BUSY leaves admission open. It must perform one bounded owner drain pass and write real pending pins, with producer stopped before OK. Existing post-seal preflight remains unchanged.
 *
 * @param abi abi.
 * @param guard guard.
 * @param callback callback.
 * @param userdata userdata.
 *
 * @return Godot Error; installation fails without publication.
 */
typedef int32_t (*GDExtensionInterfaceDeviceExitRegisterPrepareV1)(const GDExtensionDeviceExitAbi *abi, GDExtensionDeviceExitGuardToken guard, GDExtensionDeviceExitPrepareFnV1 callback, void *userdata);

/**
 * @name device_session_production_prepare_host_view_v1
 * @since 4.8
 *
 * Original paid action Host-view receiver. Prepare records the approved tuple; paid OBSERVE authenticates actual native camera/viewport and issues shared native identity. Read returns exact association after owner consumed control. No U05 proof from numeric input.
 *
 * @param request Original accepted request.
 * @param action_ordinal Original NativeOutput action ordinal.
 * @param input Aligned original input.
 *
 * @return Native status. Read output untouched on failure.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionPrepareHostViewV1)(GDExtensionProductionRequestRefV1 request, uint64_t action_ordinal, const GDExtensionProductionHostViewInputV1 *input);

/**
 * @name device_session_production_read_host_view_v1
 * @since 4.8
 *
 * Original paid action Host-view receiver. Prepare records the approved tuple; paid OBSERVE authenticates actual native camera/viewport and issues shared native identity. Read returns exact association after owner consumed control. No U05 proof from numeric input.
 *
 * @param request Original accepted request.
 * @param action_ordinal Original NativeOutput action ordinal.
 * @param out Aligned writable association result.
 *
 * @return Native status. Read output untouched on failure.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionReadHostViewV1)(GDExtensionProductionRequestRefV1 request, uint64_t action_ordinal, GDExtensionProductionHostViewAssociationV1 *out);

/**
 * @name device_session_output_consumed_borrow_v2
 * @since 4.7
 *
 * Drive or peek one original fixed OBSERVE control for an existing pre-reserved proof view. Borrow only succeeds for the original actual covering color/list-close receipt; no post-effect allocation or current-camera reacquisition. Changed selector refuses while in flight. A zero budget never schedules or admits.
 *
 * @param request Synchronous aligned nonaliasing POD; no caller pointer survives. Output remains untouched on admission failure.
 * @param selector Synchronous aligned nonaliasing POD; no caller pointer survives. Output remains untouched on admission failure.
 * @param budget Synchronous aligned nonaliasing POD; no caller pointer survives. Output remains untouched on admission failure.
 * @param out_result Synchronous aligned nonaliasing output; no caller pointer survives. Fresh-zero before every call. A projection written by this call has abi_version 2, struct_size equal to sizeof(GDExtensionOutputConsumedBorrowResultV2), and reserved zero. Admission failure and outstanding-control ERR_BUSY leave it untouched.
 *
 * @return ERR_BUSY alone does not identify control disposition. PENDING, QUEUED or EXECUTING leaves output untouched; a RETURNED result writes an ABI2 projection, even for ERR_BUSY. Failed projections are already OWNER_CONSUMED; the next same-leaf call is a fresh turn. Successful borrow remains RETURNED and requires exact read using proof.ref.borrow_control_serial. Fresh-zero output every call and validate its complete ABI2 header. Use production_control_receipt_v1 with the retained expected original serial to verify state and result_valid before another accepted control can replace it; report.service_control_serial is not populated in this path. Outstanding original budget is immutable. No logical promotion, native completion or source acknowledgment is issued.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionOutputConsumedBorrowV2)(GDExtensionProductionRequestRefV1 request, const GDExtensionOutputConsumedSelectorV2 *selector, const GDExtensionProductionServiceBudgetV1 *budget, GDExtensionOutputConsumedBorrowResultV2 *out_result);

/**
 * @name device_session_output_consumed_read_v2
 * @since 4.7
 *
 * Exact main owner consume of the successful original RETURNED borrow result. Wrong ref preserves RETURNED and replay rejects. The original proof ViewCell remains strongly retained across this transition and cancel; the consumer must separately consume RECORD and ACK every segment before Pending promotion.
 *
 * @param request Synchronous aligned nonaliasing POD; no caller pointer survives. Output remains untouched on admission failure.
 * @param expected Synchronous aligned nonaliasing POD; no caller pointer survives. Output remains untouched on admission failure.
 * @param out_proof Synchronous aligned nonaliasing POD; no caller pointer survives. Output remains untouched on admission failure.
 *
 * @return Successful exact read copies the original proof and changes the original borrow control to OWNER_CONSUMED. ERR_BUSY leaves output untouched while that control is not RETURNED. Wrong ref preserves RETURNED; replay rejects. No logical promotion, native completion or source acknowledgment is issued.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionOutputConsumedReadV2)(GDExtensionProductionRequestRefV1 request, const GDExtensionOutputConsumedProofRefV2 *expected, GDExtensionOutputConsumedProofV2 *out_proof);

/**
 * @name device_session_output_consumed_return_v2
 * @since 4.7
 *
 * Return only the exact consumed proof metadata edge through original paid OBSERVE. Requires the original successful exact read. Native uses, predecessor gather/source return, physical and code clocks remain independent. Retry this same leaf only while the original turn is outstanding; retrieving any returned cleanup projection consumes that turn, including ERR_BUSY.
 *
 * @param request Synchronous aligned nonaliasing POD; no caller pointer survives. Output remains untouched on admission failure.
 * @param expected Synchronous aligned nonaliasing POD; no caller pointer survives. Output remains untouched on admission failure.
 * @param budget Synchronous aligned nonaliasing POD; no caller pointer survives. Output remains untouched on admission failure.
 * @param out_report Synchronous aligned nonaliasing output; no caller pointer survives. Fresh-zero before every call. A projection written by this call has abi_version 2, struct_size equal to sizeof(GDExtensionProductionReportV2), and reserved zero. Admission failure and outstanding-control ERR_BUSY leave it untouched.
 *
 * @return ERR_BUSY alone does not identify control disposition. PENDING, QUEUED or EXECUTING leaves output untouched; a RETURNED result writes an ABI2 report, even for ERR_BUSY, and is already OWNER_CONSUMED on return. The next same-leaf call is a fresh turn. Fresh-zero output every call and validate its complete ABI2 header. Use production_control_receipt_v1 with the retained expected original serial to verify state and result_valid before another accepted control can replace it; report.service_control_serial is not populated in this path. Outstanding original budget is immutable. No native completion or source acknowledgment is issued.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionOutputConsumedReturnV2)(GDExtensionProductionRequestRefV1 request, const GDExtensionOutputConsumedProofRefV2 *expected, const GDExtensionProductionServiceBudgetV1 *budget, GDExtensionProductionReportV2 *out_report);

/**
 * @name device_session_output_consumed_prepare_v2
 * @since 4.7
 *
 * Opt in one original prefreeze output acquire action to V2 proof transport. This reserves no new capacity: original metadata demand must quote a persistent proof ViewCell plus every simultaneous renderer view and the corresponding original use cells. V1 actions remain unchanged. Original acquire requires at least two quoted ViewCells and normal pipeline borrower headroom; the actual first camera view reserves the proof before native consumer append.
 *
 * @param request Original request owner.
 * @param selector Synchronous aligned selector naming the original acquire action before freeze.
 *
 * @return Original checked preparation result. No registry, native pin, receipt or capacity is created.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionOutputConsumedPrepareV2)(GDExtensionProductionRequestRefV1 request, const GDExtensionOutputConsumedSelectorV2 *selector);

/**
 * @name device_session_production_prepare_output_v2
 * @since 4.7
 *
 * Versioned predecessor READ binding through the original paid fixed OBSERVE control; no inline native work on metadata thread. Requires a read consumed proof and exact accepted source locator.
 *
 * @param request Original request input.
 * @param prepare Original prepare input.
 * @param budget Original budget input.
 *
 * @return Original preparation status; success does not grant native acquisition or GPU completion.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionPrepareOutputV2)(GDExtensionProductionRequestRefV1 request, const GDExtensionProductionOutputPrepareV2 *prepare, const GDExtensionProductionServiceBudgetV1 *budget);

/**
 * @name device_session_production_prepare_output_plan_v2
 * @since 4.7
 *
 * Versioned predecessor READ binding through the original paid fixed OBSERVE control; no inline native work on metadata thread. Requires a read consumed proof and exact accepted source locator.
 *
 * @param request Original request input.
 * @param plan Original plan input.
 * @param budget Original budget input.
 *
 * @return Original preparation status; success does not grant native acquisition or GPU completion.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionPrepareOutputPlanV2)(GDExtensionProductionRequestRefV1 request, const GDExtensionProductionOutputPlanV2 *plan, const GDExtensionProductionServiceBudgetV1 *budget);

/**
 * @name device_session_output_metadata_selection_query_v2
 * @since 4.7
 *
 * Pure finite CPU slab sizing; no allocation or native selection occurs.
 *
 * @param input Synchronous aligned DTO or original owner locator.
 * @param out_requirements Synchronous aligned DTO or original owner locator.
 *
 * @return Checked original status. Success never issues material qualification or GPU completion.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionOutputMetadataSelectionQueryV2)(const GDExtensionOutputMetadataSelectionQueryV2 *input, GDExtensionOutputMetadataSelectionRequirementsV2 *out_requirements);

/**
 * @name device_session_output_metadata_selection_prepare_v2
 * @since 4.7
 *
 * Original protected RT RECORD only. Acquires CPU native selection leases in exactly one prequoted original OUTPUT_METADATA allocation. Persistent Host/Binding input/result custody must survive the asynchronous original RECORD. On partial failure the returned nonempty locator still owns its prefix until original RELEASE.
 *
 * @param physical_context Synchronous aligned DTO or original owner locator.
 * @param request Synchronous aligned DTO or original owner locator.
 * @param input Synchronous aligned DTO or original owner locator.
 * @param out_result Synchronous aligned DTO or original owner locator.
 *
 * @return Checked original status. Success never issues material qualification or GPU completion.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionOutputMetadataSelectionPrepareV2)(GDExtensionProductionPhysicalContextRefV1 physical_context, GDExtensionProductionRequestRefV1 request, const GDExtensionOutputMetadataSelectionPrepareV2 *input, GDExtensionOutputMetadataSelectionResultV2 *out_result);

/**
 * @name device_session_output_metadata_selection_read_v2
 * @since 4.7
 *
 * Original RT only. Reads one flattened selected surface and its material from the same original live attachment/request/slab. Failure leaves output unchanged. Host/Binding returns this immutable observation to main through its original persistent query custody.
 *
 * @param attachment Synchronous aligned DTO or original owner locator.
 * @param selection Synchronous aligned DTO or original owner locator.
 * @param surface Synchronous aligned DTO or original owner locator.
 * @param out_observation Synchronous aligned DTO or original owner locator.
 *
 * @return Checked original status. Success never issues material qualification or GPU completion.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionOutputMetadataSelectionReadV2)(GDExtensionProductionAttachmentRefV1 attachment, GDExtensionOutputMetadataSelectionRefV2 selection, uint32_t surface, GDExtensionOutputMetadataSelectionObservationV2 *out_observation);

/**
 * @name device_session_output_metadata_selection_return_v2
 * @since 4.7
 *
 * Main or original RT metadata close request. Seals reads and CreateV4 immediately, then returns BUSY until the same original request RELEASE has checked and returned its material/mesh selection prefix and CPU slab under the original paid service budget. Repeated close is idempotent while that request remains live; after request reclaim the locator is stale.
 *
 * @param attachment Synchronous aligned DTO or original owner locator.
 * @param selection Synchronous aligned DTO or original owner locator.
 *
 * @return Checked original status. Success never issues material qualification or GPU completion.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionOutputMetadataSelectionReturnV2)(GDExtensionProductionAttachmentRefV1 attachment, GDExtensionOutputMetadataSelectionRefV2 selection);

/**
 * @name device_session_output_create_v4
 * @since 4.7
 *
 * Original CreateV3 path plus checked early same-selection association. Result retains the ordinary generation close obligation even if a post-acquire mismatch is rejected.
 *
 * @param physical_context Synchronous aligned DTO or original owner locator.
 * @param request Synchronous aligned DTO or original owner locator.
 * @param input Synchronous aligned DTO or original owner locator.
 * @param out_result Synchronous aligned DTO or original owner locator.
 *
 * @return Checked original status. Success never issues material qualification or GPU completion.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionOutputCreateV4)(GDExtensionProductionPhysicalContextRefV1 physical_context, GDExtensionProductionRequestRefV1 request, const GDExtensionOutputCreateInputV4 *input, GDExtensionOutputCreateResultV1 *out_result);

/**
 * @name device_session_output_scene_query_commit_v2
 * @since 4.7
 *
 * Original protected RECORD only; joins the same attachment retained SceneConnectionRow and exact original receiver/base/generation root before atomic bounds/pose commit. Transform uses basis columns then origin, three doubles each.
 *
 * @param physical_context Synchronous aligned DTO or original owner locator.
 * @param request Synchronous aligned DTO or original owner locator.
 * @param input Synchronous aligned DTO or original owner locator.
 * @param out_result Synchronous aligned DTO or original owner locator.
 *
 * @return Checked original status. Success never issues material qualification or GPU completion.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionOutputSceneQueryCommitV2)(GDExtensionProductionPhysicalContextRefV1 physical_context, GDExtensionProductionRequestRefV1 request, const GDExtensionOutputSceneQueryCommitV2 *input, GDExtensionOutputSceneQueryResultV2 *out_result);

/**
 * @name device_session_output_device_facts_v2
 * @since 4.7
 *
 * Original owner thread and existing exporting DeviceSession authorization only. Issues exact canonical descriptor from authenticated original Registry/DeviceSessionKey; no allocation, registration or capacity. Output untouched on error. Copy device_tag unchanged into original Create and retain original session; the token never substitutes for native device/backing validation.
 *
 * @param context Original authenticated owner or synchronous aligned nonaliasing DTO.
 * @param session Original authenticated owner or synchronous aligned nonaliasing DTO.
 * @param out_facts Original authenticated owner or synchronous aligned nonaliasing DTO.
 *
 * @return Native checked status; success only copies original device facts.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionOutputDeviceFactsV2)(GDExtensionDeviceRegistryContextRef context, GDExtensionOriginalDeviceSessionRef session, GDExtensionOutputDeviceFactsV2 *out_facts);

/**
 * @name device_session_output_selection_facts_v2
 * @since 4.7
 *
 * Original RT only, same attachment/request/SelectionV2 slab. Instance192 returns finite native mesh observation and exact zero geometry_bounds independently of shader readiness. Pass classification uses actual native compiled shader: opaque or transparent, depth/shadow when opaque or depth-in-alpha, probe; explicit requested mask must be wholly eligible. Check pass_status before masks. Zero requested mask observes eligibility only. Indexed144 explicitly returns ERR_UNAVAILABLE because no original generated-geometry envelope owner exists here; it does not block Instance192. Failure leaves output untouched. Original query custody must retain/transport the result to main, not dereference native objects there. Eligibility never authorizes Early/Late, physical pipeline/renderer window or material qualification.
 *
 * @param attachment Original authenticated owner or synchronous aligned nonaliasing DTO.
 * @param selection Original authenticated owner or synchronous aligned nonaliasing DTO.
 * @param input Original authenticated owner or synchronous aligned nonaliasing DTO.
 * @param out_facts Original authenticated owner or synchronous aligned nonaliasing DTO.
 *
 * @return Checked observation status. OK permits mesh observation; pass_status separately gates pass fields. Explicit unsupported intent is pass_status ERR_UNAVAILABLE with selected mask zero; never silent reduction.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionOutputSelectionFactsV2)(GDExtensionProductionAttachmentRefV1 attachment, GDExtensionOutputMetadataSelectionRefV2 selection, const GDExtensionOutputSelectionFactsInputV2 *input, GDExtensionOutputSelectionFactsV2 *out_facts);

/**
 * @name device_session_production_prepare_native_record_v2
 * @since 4.7
 *
 * Opt in the same original request to execute only its prepared/frozen actions through Core in an authenticated genuine native camera invocation. Must precede freeze. CQMT waits on the same queued control and does not run this facet outside that window. Existing Runtime preparation/resource/config owners remain required; no empty generation is synthesized.
 *
 * @param request Synchronous aligned POD belonging to the same original owner; no caller pointer survives.
 *
 * @return Original checked result. No renderer window, resource/config issuer, source acknowledgment or logical application is created.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionPrepareNativeRecordV2)(GDExtensionProductionRequestRefV1 request);

/**
 * @name device_session_production_prepared_result_read_v2
 * @since 4.7
 *
 * Exact once read of one original action result after its same original RECORD result was consumed. Request/control/action/canonicalrange/substep must all match. Wrong selector preserves result and output; replay refuses. Unread real replies block the next RECORD and final RELEASE but may be read after cancellation. ACK and source/physical/code return remain separate.
 *
 * @param request Synchronous aligned POD belonging to the same original owner; no caller pointer survives.
 * @param expected Synchronous aligned POD belonging to the same original owner; no caller pointer survives.
 * @param out_result Synchronous aligned POD belonging to the same original owner; no caller pointer survives.
 *
 * @return Original checked result. No renderer window, resource/config issuer, source acknowledgment or logical application is created.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionPreparedResultReadV2)(GDExtensionProductionRequestRefV1 request, const GDExtensionProductionPreparedResultRefV2 *expected, GDExtensionProductionPreparedActionResultV2 *out_result);

/**
 * @name device_session_production_stage_output_plan_v2
 * @since 4.7
 *
 * Explicit camera-independent staged plan. Requires original retained source READ and Host association/subscription. All camera/current/previous/present fields must be zero; previous consumed history is not accepted by this initial stage. Pre-draw native prepare seals and prices static rows; genuine selected renderer invocation completes camera-only fields and executes original RECORD. Ordinary prepare, Acquire and freeze remain strict. Budget is immutable through the original RECORD and expires even with no draw. One staged output anchor per request; owner consumes results and acknowledges explicitly.
 *
 * @param request Original request input.
 * @param plan Original plan input.
 * @param budget Original budget input.
 *
 * @return Original preparation status; success does not grant native acquisition or GPU completion.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionStageOutputPlanV2)(GDExtensionProductionRequestRefV1 request, const GDExtensionProductionOutputPlanV1 *plan, const GDExtensionProductionServiceBudgetV1 *budget);

/**
 * @name device_session_production_record_consume_v3
 * @since 4.7
 *
 * Exact original RECORD consume returning ABI3 service-bearing report. Does not consume/apply typed action results or ACK segment. Same original serial consumed once; V2 remains available for legacy callers.
 *
 * @param request Synchronous checked request.
 * @param control_serial Synchronous checked control serial.
 * @param out_report Synchronous checked out report.
 *
 * @return Original checked native result. Returned API calls and Core physical contexts do not issue original Runtime logical permissions.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionRecordConsumeV3)(GDExtensionProductionRequestRefV1 request, uint64_t control_serial, GDExtensionProductionRecordReportV3 *out_report);

/**
 * @name device_session_production_stage_camera_payload_v2
 * @since 4.7
 *
 * Stage exact original camera payload upload bindings before native static seal. No native effect or camera capture is created here.
 *
 * @param request Original request input.
 * @param input Explicit original revision-1 upload bindings.
 *
 * @return Original preparation status; success does not grant native acquisition or GPU completion.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionStageCameraPayloadV2)(GDExtensionProductionRequestRefV1 request, const GDExtensionProductionCameraPayloadV1 *input);

/**
 * @name device_session_production_read_staged_output_input_v2
 * @since 4.7
 *
 * After exact original RECORD consume, read the actual same-capture completed original Acquire preparation including current/actual-previous camera and present sequence. No new identity or receipt; failure leaves output untouched.
 *
 * @param p_request Original typed request.
 * @param p_action_ordinal Original typed action ordinal.
 * @param r_input Original typed transform.
 *
 * @return Original native admission or contract result.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionReadStagedOutputInputV2)(GDExtensionProductionRequestRefV1 p_request, uint64_t p_action_ordinal, GDExtensionProductionOutputPrepareV1 *r_input);

/**
 * @name device_session_production_prepare_native_batch_v2
 * @since 4.7
 *
 * Opt into finite same-original-control native subframes after static preparation and before RECORD. Original immutable stage pass/receipt budget pays each genuine physical frame; unchanged per-frame64/scene cap. No owner consume/application/ACK in renderer; actual results remain retained in paid original rows.
 *
 * @param request Synchronous aligned POD belonging to the same original owner; no caller pointer survives.
 *
 * @return Original checked result. No renderer window, resource/config issuer, source acknowledgment or logical application is created.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionPrepareNativeBatchV2)(GDExtensionProductionRequestRefV1 request);

/**
 * @name device_session_production_native_batch_read_v2
 * @since 4.7
 *
 * Read the next exact original retained batch result only after original RECORD consume. Cursor is opaque custody reference, not authority. Sequential exact-once reads; no side effects or ACK.
 *
 * @param request Synchronous checked request.
 * @param control_serial Synchronous checked control serial.
 * @param p_cursor Zero for first result; then exact previous next_cursor.
 * @param out_report Synchronous checked out report.
 *
 * @return Original checked native result. Returned API calls and Core physical contexts do not issue original Runtime logical permissions.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionNativeBatchReadV2)(GDExtensionProductionRequestRefV1 request, uint64_t control_serial, uint64_t p_cursor, GDExtensionProductionNativeBatchItemV1 *out_report);

/**
 * @name device_session_production_stage_output_plan_with_predecessor_v2
 * @since 4.7
 *
 * Stage explicit previous-consumed intent before subscription. Retain the original genuine ConsumedProof/ViewLease via the existing original OBSERVE, then stage structural output. Current/actual-previous camera stays unresolved until genuine invocation, where exact prior camera/presentation equality is revalidated before effects. Original immutable native slice and predecessor OBSERVE turn slice are distinct.
 *
 * @param request Original request input.
 * @param plan Original plan input.
 * @param p_stage_budget Original budget input.
 * @param p_predecessor_turn_budget Original finite OBSERVE turn budget; does not refill stage budget.
 *
 * @return Original preparation status; success does not grant native acquisition or GPU completion.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionStageOutputPlanWithPredecessorV2)(GDExtensionProductionRequestRefV1 request, const GDExtensionProductionOutputPlanV2 *plan, const GDExtensionProductionServiceBudgetV1 *p_stage_budget, const GDExtensionProductionServiceBudgetV1 *p_predecessor_turn_budget);

/**
 * @name device_session_qualification_scene_depth_service_reserve_v2
 * @since 4.7
 *
 * Original qualification cell exact Height paid service. No new RT kind. Native grant uses original absolute Godot OS deadline; owner synchronous pass time is not transferred or extended. Source/submission authority is authenticated independently.
 *
 * CURRENT mode 1 observes without requesting return. RETIRE3 mode 2 services only original range before separate allocation-owner retirement. Usage is cumulative across observed Busy retries under the same finite immutable grant.
 *
 * @param bridge Copied aligned nonaliasing original value/span; no caller bytes retained.
 * @param request Copied aligned nonaliasing original value/span; no caller bytes retained.
 * @param grant Copied aligned nonaliasing original value/span; no caller bytes retained.
 * @param out_original_rt Copied aligned nonaliasing original value/span; no caller bytes retained.
 *
 * @return Typed transport status. Native completion, row admission, cumulative actual usage and quota/deadline exhaustion are distinct returned facts.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationSceneDepthServiceReserveV2)(GDExtensionQualificationBridgeRefV1 bridge, const GDExtensionSceneDepthServiceRequestV2 *request, const GDExtensionQualificationServiceGrantV2 *grant, GDExtensionQualificationRtRefV1 *out_original_rt);

/**
 * @name device_session_qualification_scene_depth_service_activate_v2
 * @since 4.7
 *
 * Original qualification cell exact Height paid service. No new RT kind. Native grant uses original absolute Godot OS deadline; owner synchronous pass time is not transferred or extended. Source/submission authority is authenticated independently.
 *
 * CURRENT mode 1 observes without requesting return. RETIRE3 mode 2 services only original range before separate allocation-owner retirement. Usage is cumulative across observed Busy retries under the same finite immutable grant.
 *
 * @param bridge Copied aligned nonaliasing original value/span; no caller bytes retained.
 * @param original_rt Copied aligned nonaliasing original value/span; no caller bytes retained.
 * @param expected_grant_serial Copied aligned nonaliasing original value/span; no caller bytes retained.
 * @param expected_execution_serial Copied aligned nonaliasing original value/span; no caller bytes retained.
 *
 * @return Typed transport status. Native completion, row admission, cumulative actual usage and quota/deadline exhaustion are distinct returned facts.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationSceneDepthServiceActivateV2)(GDExtensionQualificationBridgeRefV1 bridge, GDExtensionQualificationRtRefV1 original_rt, uint64_t expected_grant_serial, uint64_t expected_execution_serial);

/**
 * @name device_session_qualification_scene_depth_service_observe_v2
 * @since 4.7
 *
 * Original qualification cell exact Height paid service. No new RT kind. Native grant uses original absolute Godot OS deadline; owner synchronous pass time is not transferred or extended. Source/submission authority is authenticated independently.
 *
 * CURRENT mode 1 observes without requesting return. RETIRE3 mode 2 services only original range before separate allocation-owner retirement. Usage is cumulative across observed Busy retries under the same finite immutable grant.
 *
 * @param bridge Copied aligned nonaliasing original value/span; no caller bytes retained.
 * @param original_rt Copied aligned nonaliasing original value/span; no caller bytes retained.
 * @param out Copied aligned nonaliasing original value/span; no caller bytes retained.
 * @param allocation_report Copied aligned nonaliasing original value/span; no caller bytes retained.
 *
 * @return Typed transport status. Native completion, row admission, cumulative actual usage and quota/deadline exhaustion are distinct returned facts.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationSceneDepthServiceObserveV2)(GDExtensionQualificationBridgeRefV1 bridge, GDExtensionQualificationRtRefV1 original_rt, GDExtensionQualificationServiceObservationV2 *out, GDExtensionStorageSceneDepthReportV1 *allocation_report);

/**
 * @name device_session_qualification_record_method_v1
 * @since 4.7
 *
 * Same original ticket and operation with three additional original Method gate readbacks; preserves original bounded admission, completion facts, result copy and retirement. No Numeric schema or qualifier is created.
 *
 * @param bridge Synchronous in-process bridge.
 * @param ticket Synchronous in-process ticket.
 * @param request Synchronous in-process request.
 * @param out_report Synchronous in-process out report.
 *
 * @return Original native Error/reason observation. A possible effect requires a retained original child/ticket, never an unowned bare error.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationRecordMethodV1)(GDExtensionQualificationBridgeRefV1 bridge, GDExtensionQualificationTicketRefV1 ticket, const GDExtensionQualificationMethodRecordV1 *request, GDExtensionQualificationMethodReportV1 *out_report);

/**
 * @name device_session_qualification_observe_method_v1
 * @since 4.7
 *
 * Same original ticket and operation with three additional original Method gate readbacks; preserves original bounded admission, completion facts, result copy and retirement. No Numeric schema or qualifier is created.
 *
 * @param bridge Synchronous in-process bridge.
 * @param ticket Synchronous in-process ticket.
 * @param budget Synchronous in-process budget.
 * @param out_report Synchronous in-process out report.
 *
 * @return Original native Error/reason observation. A possible effect requires a retained original child/ticket, never an unowned bare error.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionQualificationObserveMethodV1)(GDExtensionQualificationBridgeRefV1 bridge, GDExtensionQualificationTicketRefV1 ticket, const GDExtensionQualificationBudgetV1 *budget, GDExtensionQualificationMethodReportV1 *out_report);

/**
 * @name device_session_production_stage_output_plan_with_history_guard_v2
 * @since 4.7
 *
 * Stage the original WithPrevious plan and exact prior K GuardStatus32 companion through the same predecessor OBSERVE. The original typed source/mapping/unit chain proves semantic consumers; Core authenticates the consumed view, exact K source, readonly reflection, all actual accepted dispatch uses between Acquire and Finalize, and native lifetime. No RID, writable grant, singleton consumer, native execution or completion is supplied.
 *
 * @param request Original request input.
 * @param plan Original plan input.
 * @param guard_resource_slot Original accepted retained-prepared GuardStatus32 row, including its exact Acquire dependency and all original typed consumer READ32 spans.
 * @param p_stage_budget Original budget input.
 * @param p_predecessor_turn_budget Original finite OBSERVE turn budget; does not refill stage budget.
 *
 * @return Original preparation status; success does not grant native acquisition or GPU completion.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionStageOutputPlanWithHistoryGuardV2)(GDExtensionProductionRequestRefV1 request, const GDExtensionProductionOutputPlanV2 *plan, uint32_t guard_resource_slot, const GDExtensionProductionServiceBudgetV1 *p_stage_budget, const GDExtensionProductionServiceBudgetV1 *p_predecessor_turn_budget);

/**
 * @name device_session_production_stage_output_history_fallback_v2
 * @since 4.7
 *
 * Retain exactly one original qualified WithoutPrevious fallback in the same paid request, before its genuine camera invocation. The original caller proves source mapping. Spare paid action cells are preparation only, never added to action_count, facts or completion. Both resource banks remain retained through original release.
 *
 * @param request Original accepted request.
 * @param plan Original WithoutPrevious fallback plan.
 * @param actions Same logical ordinals and canonical range; immutable fallback preparation rows.
 * @param action_count Exactly original logical action count; paid spare capacity must retain another full preparation bank.
 *
 * @return Original preparation status; success does not grant native acquisition or GPU completion.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionStageOutputHistoryFallbackV2)(GDExtensionProductionRequestRefV1 request, const GDExtensionProductionOutputPlanV1 *plan, const GDExtensionProductionActionV2 *actions, uint32_t action_count);

/**
 * @name device_session_production_prepare_history_fallback_dispatch_v2
 * @since 4.7
 *
 * Prepare only the one predeclared WithoutPrevious fallback of the same original request. All payload bytes are retained and charged; no execution, selected-source grant, fact or unused-cancel is issued.
 *
 * @param request Synchronous checked request.
 * @param dispatch Synchronous checked dispatch.
 * @param push Synchronous checked push.
 * @param push_bytes Synchronous checked push bytes.
 *
 * @return Original checked native result. Returned API calls and Core physical contexts do not issue original Runtime logical permissions.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionPrepareHistoryFallbackDispatchV2)(GDExtensionProductionRequestRefV1 request, const GDExtensionProductionDispatchV2 *dispatch, const void *push, uint32_t push_bytes);

/**
 * @name device_session_production_prepare_history_fallback_upload_v2
 * @since 4.7
 *
 * Prepare only the one predeclared WithoutPrevious fallback of the same original request. All payload bytes are retained and charged; no execution, selected-source grant, fact or unused-cancel is issued.
 *
 * @param request Synchronous checked request.
 * @param upload Synchronous checked upload.
 * @param payload Synchronous checked payload.
 * @param payload_bytes Synchronous checked payload bytes.
 *
 * @return Original checked native result. Returned API calls and Core physical contexts do not issue original Runtime logical permissions.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionPrepareHistoryFallbackUploadV2)(GDExtensionProductionRequestRefV1 request, const GDExtensionProductionUploadV2 *upload, const void *payload, uint64_t payload_bytes);

/**
 * @name device_session_production_stage_output_history_static_v2
 * @since 4.7
 *
 * Retain immutable original history policy and CPU consumed-token comparison in this same request. Repeat with identical input after owner consumption to read its full final mask. No OBSERVE, GPU old read, source grant, second window or added action is issued.
 *
 * @param request Original request history input or read-only state projection.
 * @param action_ordinal Original request history input or read-only state projection.
 * @param input Original request history input or read-only state projection.
 * @param out_state Original request history input or read-only state projection.
 *
 * @return Original native validation result.
 */
typedef GDExtensionDeviceSessionResultV1 (*GDExtensionInterfaceDeviceSessionProductionStageOutputHistoryStaticV2)(GDExtensionProductionRequestRefV1 request, uint64_t action_ordinal, const GDExtensionProductionOutputHistoryStaticV2 *input, GDExtensionProductionOutputHistoryStateV2 *out_state);

#ifdef __cplusplus
}
#endif
