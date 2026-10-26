/**************************************************************************/
/*  gdextension.cpp                                                       */
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

#include "gdextension.h"
#include "gdextension.compat.inc"

#include "core/config/project_settings.h"
#include "core/extension/gdextension_library_loader.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/object/method_bind.h"

extern void gdextension_setup_interface();
extern GDExtensionInterfaceFunctionPtr gdextension_get_proc_address(const char *p_name);
extern GDExtensionInterfaceFunctionPtr gdextension_get_guarded_proc_address(const char *p_name);

typedef GDExtensionBool (*GDExtensionLegacyInitializationFunction)(void *p_interface, GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization);

String GDExtension::get_extension_list_config_file() {
	return ProjectSettings::get_singleton()->get_project_data_path().path_join("extension_list.cfg");
}

class GDExtensionMethodBind : public MethodBind {
	GDExtensionDeviceExit::RecordRef library_exit_ref;
	GDExtensionClassMethodCall call_func;
	GDExtensionClassMethodValidatedCall validated_call_func;
	GDExtensionClassMethodPtrCall ptrcall_func;
	void *method_userdata;
	bool vararg;
	uint32_t argument_count;
	PropertyInfo return_value_info;
	bool return_is_refcounted = false;
	GodotTypeInfo::Metadata return_value_metadata = GodotTypeInfo::METADATA_NONE;
	List<PropertyInfo> arguments_info;
	List<GodotTypeInfo::Metadata> arguments_metadata;

#ifdef TOOLS_ENABLED
	friend class GDExtension;

	StringName name;
	bool is_reloading = false;
	bool valid = true;
#endif

protected:
	virtual Variant::Type _gen_argument_type(int p_arg) const override {
		if (p_arg < 0) {
			return return_value_info.type;
		} else {
			return arguments_info.get(p_arg).type;
		}
	}
	virtual PropertyInfo _gen_argument_type_info(int p_arg) const override {
		if (p_arg < 0) {
			return return_value_info;
		} else {
			return arguments_info.get(p_arg);
		}
	}

	void encode_closed_return(void *r_ret) const {
		if (!r_ret || !has_return()) {
			return;
		}
		// Core ABI encoders assign into the caller's initialized return storage.
		// Integer/real metadata uses the canonical int64/double ptrcall lanes.
		switch (return_value_info.type) {
			case Variant::NIL:
				PtrToArg<Variant>::encode(Variant{}, r_ret);
				break;
			case Variant::BOOL:
				PtrToArg<bool>::encode(bool{}, r_ret);
				break;
			case Variant::INT:
				PtrToArg<int64_t>::encode(int64_t{}, r_ret);
				break;
			case Variant::FLOAT:
				PtrToArg<double>::encode(double{}, r_ret);
				break;
			case Variant::STRING:
				PtrToArg<String>::encode(String{}, r_ret);
				break;
			case Variant::VECTOR2:
				PtrToArg<Vector2>::encode(Vector2{}, r_ret);
				break;
			case Variant::VECTOR2I:
				PtrToArg<Vector2i>::encode(Vector2i{}, r_ret);
				break;
			case Variant::RECT2:
				PtrToArg<Rect2>::encode(Rect2{}, r_ret);
				break;
			case Variant::RECT2I:
				PtrToArg<Rect2i>::encode(Rect2i{}, r_ret);
				break;
			case Variant::VECTOR3:
				PtrToArg<Vector3>::encode(Vector3{}, r_ret);
				break;
			case Variant::VECTOR3I:
				PtrToArg<Vector3i>::encode(Vector3i{}, r_ret);
				break;
			case Variant::TRANSFORM2D:
				PtrToArg<Transform2D>::encode(Transform2D{}, r_ret);
				break;
			case Variant::VECTOR4:
				PtrToArg<Vector4>::encode(Vector4{}, r_ret);
				break;
			case Variant::VECTOR4I:
				PtrToArg<Vector4i>::encode(Vector4i{}, r_ret);
				break;
			case Variant::PLANE:
				PtrToArg<Plane>::encode(Plane{}, r_ret);
				break;
			case Variant::QUATERNION:
				PtrToArg<Quaternion>::encode(Quaternion{}, r_ret);
				break;
			case Variant::AABB:
				PtrToArg<AABB>::encode(AABB{}, r_ret);
				break;
			case Variant::BASIS:
				PtrToArg<Basis>::encode(Basis{}, r_ret);
				break;
			case Variant::TRANSFORM3D:
				PtrToArg<Transform3D>::encode(Transform3D{}, r_ret);
				break;
			case Variant::PROJECTION:
				PtrToArg<Projection>::encode(Projection{}, r_ret);
				break;
			case Variant::COLOR:
				PtrToArg<Color>::encode(Color{}, r_ret);
				break;
			case Variant::STRING_NAME:
				PtrToArg<StringName>::encode(StringName{}, r_ret);
				break;
			case Variant::NODE_PATH:
				PtrToArg<NodePath>::encode(NodePath{}, r_ret);
				break;
			case Variant::RID:
				PtrToArg<RID>::encode(RID{}, r_ret);
				break;
			case Variant::OBJECT:
				if (return_is_refcounted) {
					PtrToArg<Ref<RefCounted>>::encode(Ref<RefCounted>(), r_ret);
				} else {
					PtrToArg<Object *>::encode(nullptr, r_ret);
				}
				break;
			case Variant::CALLABLE:
				PtrToArg<Callable>::encode(Callable{}, r_ret);
				break;
			case Variant::SIGNAL:
				PtrToArg<Signal>::encode(Signal{}, r_ret);
				break;
			case Variant::DICTIONARY:
				PtrToArg<Dictionary>::encode(Dictionary{}, r_ret);
				break;
			case Variant::ARRAY:
				PtrToArg<Array>::encode(Array{}, r_ret);
				break;
			case Variant::PACKED_BYTE_ARRAY:
				PtrToArg<PackedByteArray>::encode(PackedByteArray{}, r_ret);
				break;
			case Variant::PACKED_INT32_ARRAY:
				PtrToArg<PackedInt32Array>::encode(PackedInt32Array{}, r_ret);
				break;
			case Variant::PACKED_INT64_ARRAY:
				PtrToArg<PackedInt64Array>::encode(PackedInt64Array{}, r_ret);
				break;
			case Variant::PACKED_FLOAT32_ARRAY:
				PtrToArg<PackedFloat32Array>::encode(PackedFloat32Array{}, r_ret);
				break;
			case Variant::PACKED_FLOAT64_ARRAY:
				PtrToArg<PackedFloat64Array>::encode(PackedFloat64Array{}, r_ret);
				break;
			case Variant::PACKED_STRING_ARRAY:
				PtrToArg<PackedStringArray>::encode(PackedStringArray{}, r_ret);
				break;
			case Variant::PACKED_VECTOR2_ARRAY:
				PtrToArg<PackedVector2Array>::encode(PackedVector2Array{}, r_ret);
				break;
			case Variant::PACKED_VECTOR3_ARRAY:
				PtrToArg<PackedVector3Array>::encode(PackedVector3Array{}, r_ret);
				break;
			case Variant::PACKED_COLOR_ARRAY:
				PtrToArg<PackedColorArray>::encode(PackedColorArray{}, r_ret);
				break;
			case Variant::PACKED_VECTOR4_ARRAY:
				PtrToArg<PackedVector4Array>::encode(PackedVector4Array{}, r_ret);
				break;
			default:
				ERR_FAIL_MSG("UNSUPPORTED_ENTRY_CLASS");
		}
	}

public:
#ifdef TOOLS_ENABLED
	virtual bool is_valid() const override { return valid; }
#endif

#ifdef DEBUG_ENABLED
	virtual GodotTypeInfo::Metadata get_argument_meta(int p_arg) const override {
		if (p_arg < 0) {
			return return_value_metadata;
		} else {
			return arguments_metadata.get(p_arg);
		}
	}
#endif // DEBUG_ENABLED

	virtual Variant call(Object *p_object, const Variant **p_args, int p_arg_count, Callable::CallError &r_error) const override {
		GDExtensionDeviceExit::CallbackScope callback_scope(library_exit_ref);
		if (callback_scope.get_error() != OK) {
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
			r_error.argument = 0;
			r_error.expected = 0;
			return Variant();
		}
#ifdef TOOLS_ENABLED
		ERR_FAIL_COND_V_MSG(!valid, Variant(), vformat("Cannot call invalid GDExtension method bind '%s'. It's probably cached - you may need to restart Godot.", name));
		ERR_FAIL_COND_V_MSG(p_object && p_object->is_extension_placeholder(), Variant(), vformat("Cannot call GDExtension method bind '%s' on placeholder instance.", name));
#endif
		Variant ret;
		GDExtensionClassInstancePtr extension_instance = is_static() ? nullptr : p_object->_get_extension_instance();
		GDExtensionCallError ce{ GDEXTENSION_CALL_OK, 0, 0 };
		call_func(method_userdata, extension_instance, reinterpret_cast<GDExtensionConstVariantPtr *>(p_args), p_arg_count, (GDExtensionVariantPtr)&ret, &ce);
		r_error.error = Callable::CallError::Error(ce.error);
		r_error.argument = ce.argument;
		r_error.expected = ce.expected;
		return ret;
	}
	virtual void validated_call(Object *p_object, const Variant **p_args, Variant *r_ret) const override {
		GDExtensionDeviceExit::CallbackScope callback_scope(library_exit_ref);
		if (callback_scope.get_error() != OK) {
			if (r_ret && has_return()) {
				Callable::CallError error;
				Variant::construct(return_value_info.type, *r_ret, nullptr, 0, error);
			}
			ERR_FAIL_MSG("ENTRY_CLOSED");
		}
#ifdef TOOLS_ENABLED
		ERR_FAIL_COND_MSG(!valid, vformat("Cannot call invalid GDExtension method bind '%s'. It's probably cached - you may need to restart Godot.", name));
		ERR_FAIL_COND_MSG(p_object && p_object->is_extension_placeholder(), vformat("Cannot call GDExtension method bind '%s' on placeholder instance.", name));
#endif
		ERR_FAIL_COND_MSG(vararg, "Vararg methods don't have validated call support. This is most likely an engine bug.");
		GDExtensionClassInstancePtr extension_instance = is_static() ? nullptr : p_object->_get_extension_instance();

		if (validated_call_func) {
			// This is added here, but it's unlikely to be provided by most extensions.
			validated_call_func(method_userdata, extension_instance, reinterpret_cast<GDExtensionConstVariantPtr *>(p_args), (GDExtensionVariantPtr)r_ret);
		} else {
			// If not provided, go via ptrcall, which is faster than resorting to regular call.
			const void **argptrs = (const void **)alloca(argument_count * sizeof(void *));
			for (uint32_t i = 0; i < argument_count; i++) {
				argptrs[i] = VariantInternal::get_opaque_pointer(p_args[i]);
			}

			void *ret_opaque = nullptr;
			if (r_ret) {
				VariantInternal::initialize(r_ret, return_value_info.type);
				ret_opaque = r_ret->get_type() == Variant::NIL ? r_ret : VariantInternal::get_opaque_pointer(r_ret);
			}

			ptrcall_func(method_userdata, extension_instance, reinterpret_cast<GDExtensionConstTypePtr *>(argptrs), (GDExtensionTypePtr)ret_opaque);

			if (r_ret && r_ret->get_type() == Variant::OBJECT) {
				VariantInternal::update_object_id(r_ret);
			}
		}
	}

	virtual void ptrcall(Object *p_object, const void **p_args, void *r_ret) const override {
		GDExtensionDeviceExit::CallbackScope callback_scope(library_exit_ref);
		if (callback_scope.get_error() != OK) {
			encode_closed_return(r_ret);
			ERR_FAIL_MSG("ENTRY_CLOSED");
		}
#ifdef TOOLS_ENABLED
		ERR_FAIL_COND_MSG(!valid, vformat("Cannot call invalid GDExtension method bind '%s'. It's probably cached - you may need to restart Godot.", name));
		ERR_FAIL_COND_MSG(p_object && p_object->is_extension_placeholder(), vformat("Cannot call GDExtension method bind '%s' on placeholder instance.", name));
#endif
		ERR_FAIL_COND_MSG(vararg, "Vararg methods don't have ptrcall support. This is most likely an engine bug.");
		GDExtensionClassInstancePtr extension_instance = is_static() ? nullptr : p_object->_get_extension_instance();
		ptrcall_func(method_userdata, extension_instance, reinterpret_cast<GDExtensionConstTypePtr *>(p_args), (GDExtensionTypePtr)r_ret);
	}

	virtual bool is_vararg() const override {
		return vararg;
	}

#ifdef TOOLS_ENABLED
	bool try_update(const GDExtensionClassMethodInfo *p_method_info, GDExtensionDeviceExit::RecordRef &p_reference) {
		if (is_static() != (bool)(p_method_info->method_flags & GDEXTENSION_METHOD_FLAG_STATIC)) {
			return false;
		}

		if (vararg != (bool)(p_method_info->method_flags & GDEXTENSION_METHOD_FLAG_VARARG)) {
			return false;
		}

		if (has_return() != (bool)p_method_info->has_return_value) {
			return false;
		}

		if (has_return() && return_value_info.type != (Variant::Type)p_method_info->return_value_info->type) {
			return false;
		}

		if (has_return() && return_value_info.type == Variant::OBJECT) {
			const StringName &return_class = *reinterpret_cast<const StringName *>(p_method_info->return_value_info->class_name);
			if (return_is_refcounted != ClassDB::is_parent_class(return_class, "RefCounted")) {
				return false;
			}
		}

		if (argument_count != p_method_info->argument_count) {
			return false;
		}

		List<PropertyInfo>::ConstIterator itr = arguments_info.begin();
		for (uint32_t i = 0; i < p_method_info->argument_count; ++itr, ++i) {
			if (itr->type != (Variant::Type)p_method_info->arguments_info[i].type) {
				return false;
			}
		}

		library_exit_ref = std::move(p_reference);
		update(p_method_info);
		return true;
	}
#endif

	void update(const GDExtensionClassMethodInfo *p_method_info) {
#ifdef TOOLS_ENABLED
		name = *reinterpret_cast<StringName *>(p_method_info->name);
#endif
		method_userdata = p_method_info->method_userdata;
		call_func = p_method_info->call_func;
		validated_call_func = nullptr;
		ptrcall_func = p_method_info->ptrcall_func;
		set_name(*reinterpret_cast<StringName *>(p_method_info->name));

		if (p_method_info->has_return_value) {
			return_value_info = PropertyInfo(*p_method_info->return_value_info);
			return_is_refcounted = return_value_info.type == Variant::OBJECT && ClassDB::is_parent_class(return_value_info.class_name, "RefCounted");
			return_value_metadata = GodotTypeInfo::Metadata(p_method_info->return_value_metadata);
		}

		arguments_info.clear();
		arguments_metadata.clear();
		for (uint32_t i = 0; i < p_method_info->argument_count; i++) {
			arguments_info.push_back(PropertyInfo(p_method_info->arguments_info[i]));
			arguments_metadata.push_back(GodotTypeInfo::Metadata(p_method_info->arguments_metadata[i]));
		}

		set_hint_flags(p_method_info->method_flags);
		argument_count = p_method_info->argument_count;
		vararg = p_method_info->method_flags & GDEXTENSION_METHOD_FLAG_VARARG;
		_set_returns(p_method_info->has_return_value);
		_set_const(p_method_info->method_flags & GDEXTENSION_METHOD_FLAG_CONST);
		_set_static(p_method_info->method_flags & GDEXTENSION_METHOD_FLAG_STATIC);
#ifdef DEBUG_ENABLED
		_generate_argument_types(p_method_info->argument_count);
#endif // DEBUG_ENABLED
		set_argument_count(p_method_info->argument_count);

		Vector<Variant> defargs;
		defargs.resize(p_method_info->default_argument_count);
		for (uint32_t i = 0; i < p_method_info->default_argument_count; i++) {
			defargs.write[i] = *static_cast<Variant *>(p_method_info->default_arguments[i]);
		}

		set_default_arguments(defargs);
	}

	GDExtensionMethodBind(const GDExtensionClassMethodInfo *p_method_info, GDExtensionDeviceExit::RecordRef &&p_reference) : library_exit_ref(std::move(p_reference)) {
		update(p_method_info);
	}
};

#ifndef DISABLE_DEPRECATED
void GDExtension::_register_extension_class(GDExtensionClassLibraryPtr p_library, GDExtensionConstStringNamePtr p_class_name, GDExtensionConstStringNamePtr p_parent_class_name, const GDExtensionClassCreationInfo *p_extension_funcs) {
	const GDExtensionClassCreationInfo6 class_info6 = {
		p_extension_funcs->is_virtual, // GDExtensionBool is_virtual;
		p_extension_funcs->is_abstract, // GDExtensionBool is_abstract;
		true, // GDExtensionBool is_exposed;
		false, // GDExtensionBool is_runtime;
		nullptr, // GDExtensionConstStringPtr icon_path;
		p_extension_funcs->set_func, // GDExtensionClassSet set_func;
		p_extension_funcs->get_func, // GDExtensionClassGet get_func;
		p_extension_funcs->get_property_list_func, // GDExtensionClassGetPropertyList get_property_list_func;
		nullptr, // GDExtensionClassFreePropertyList2 free_property_list_func;
		p_extension_funcs->property_can_revert_func, // GDExtensionClassPropertyCanRevert property_can_revert_func;
		p_extension_funcs->property_get_revert_func, // GDExtensionClassPropertyGetRevert property_get_revert_func;
		nullptr, // GDExtensionClassValidateProperty validate_property_func;
		nullptr, // GDExtensionClassNotification2 notification_func;
		p_extension_funcs->to_string_func, // GDExtensionClassToString to_string_func;
		p_extension_funcs->reference_func, // GDExtensionClassReference reference_func;
		p_extension_funcs->unreference_func, // GDExtensionClassUnreference unreference_func;
		nullptr, // GDExtensionClassCreateInstance3 create_instance_func; /* this one is mandatory */
		p_extension_funcs->free_instance_func, // GDExtensionClassFreeInstance free_instance_func; /* this one is mandatory */
		nullptr, // GDExtensionClassRecreateInstance recreate_instance_func;
		nullptr, // GDExtensionClassGetVirtual2 get_virtual_func;
		nullptr, // GDExtensionClassGetVirtualCallData2 get_virtual_call_data_func;
		nullptr, // GDExtensionClassCallVirtualWithData call_virtual_with_data_func;
		p_extension_funcs->class_userdata, // void *class_userdata;
	};

	const ClassCreationDeprecatedInfo legacy = {
		false, // bool legacy_unexposed_class;
		p_extension_funcs->notification_func, // GDExtensionClassNotification notification_func;
		p_extension_funcs->free_property_list_func, // GDExtensionClassFreePropertyList free_property_list_func;
		p_extension_funcs->create_instance_func, // GDExtensionClassCreateInstance create_instance_func;
		nullptr, // GDExtensionClassCreateInstance2 create_instance2_func;
		p_extension_funcs->get_rid_func, // GDExtensionClassGetRID get_rid_func;
		p_extension_funcs->get_virtual_func, // GDExtensionClassGetVirtual get_virtual_func;
		nullptr, // GDExtensionClassGetVirtualCallData get_virtual_call_data_func;
	};
	_register_extension_class_internal(p_library, p_class_name, p_parent_class_name, &class_info6, &legacy);
}

void GDExtension::_register_extension_class2(GDExtensionClassLibraryPtr p_library, GDExtensionConstStringNamePtr p_class_name, GDExtensionConstStringNamePtr p_parent_class_name, const GDExtensionClassCreationInfo2 *p_extension_funcs) {
	const GDExtensionClassCreationInfo6 class_info6 = {
		p_extension_funcs->is_virtual, // GDExtensionBool is_virtual;
		p_extension_funcs->is_abstract, // GDExtensionBool is_abstract;
		p_extension_funcs->is_exposed, // GDExtensionBool is_exposed;
		false, // GDExtensionBool is_runtime;
		nullptr, // GDExtensionConstStringPtr icon_path;
		p_extension_funcs->set_func, // GDExtensionClassSet set_func;
		p_extension_funcs->get_func, // GDExtensionClassGet get_func;
		p_extension_funcs->get_property_list_func, // GDExtensionClassGetPropertyList get_property_list_func;
		nullptr, // GDExtensionClassFreePropertyList2 free_property_list_func;
		p_extension_funcs->property_can_revert_func, // GDExtensionClassPropertyCanRevert property_can_revert_func;
		p_extension_funcs->property_get_revert_func, // GDExtensionClassPropertyGetRevert property_get_revert_func;
		p_extension_funcs->validate_property_func, // GDExtensionClassValidateProperty validate_property_func;
		p_extension_funcs->notification_func, // GDExtensionClassNotification2 notification_func;
		p_extension_funcs->to_string_func, // GDExtensionClassToString to_string_func;
		p_extension_funcs->reference_func, // GDExtensionClassReference reference_func;
		p_extension_funcs->unreference_func, // GDExtensionClassUnreference unreference_func;
		nullptr, // GDExtensionClassCreateInstance3 create_instance_func; /* this one is mandatory */
		p_extension_funcs->free_instance_func, // GDExtensionClassFreeInstance free_instance_func; /* this one is mandatory */
		p_extension_funcs->recreate_instance_func, // GDExtensionClassRecreateInstance recreate_instance_func;
		nullptr, // GDExtensionClassGetVirtual2 get_virtual_func;
		nullptr, // GDExtensionClassGetVirtualCallData2 get_virtual_call_data_func;
		p_extension_funcs->call_virtual_with_data_func, // GDExtensionClassCallVirtualWithData call_virtual_with_data_func;
		p_extension_funcs->class_userdata, // void *class_userdata;
	};

	const ClassCreationDeprecatedInfo legacy = {
		!p_extension_funcs->is_exposed, // bool legacy_unexposed_class;
		nullptr, // GDExtensionClassNotification notification_func;
		p_extension_funcs->free_property_list_func, // GDExtensionClassFreePropertyList free_property_list_func;
		p_extension_funcs->create_instance_func, // GDExtensionClassCreateInstance create_instance_func;
		nullptr, // GDExtensionClassCreateInstance2 create_instance2_func;
		p_extension_funcs->get_rid_func, // GDExtensionClassGetRID get_rid_func;
		p_extension_funcs->get_virtual_func, // GDExtensionClassGetVirtual get_virtual_func;
		p_extension_funcs->get_virtual_call_data_func, // GDExtensionClassGetVirtualCallData get_virtual_call_data_func;
	};
	_register_extension_class_internal(p_library, p_class_name, p_parent_class_name, &class_info6, &legacy);
}

void GDExtension::_register_extension_class3(GDExtensionClassLibraryPtr p_library, GDExtensionConstStringNamePtr p_class_name, GDExtensionConstStringNamePtr p_parent_class_name, const GDExtensionClassCreationInfo3 *p_extension_funcs) {
	const GDExtensionClassCreationInfo6 class_info6 = {
		p_extension_funcs->is_virtual, // GDExtensionBool is_virtual;
		p_extension_funcs->is_abstract, // GDExtensionBool is_abstract;
		p_extension_funcs->is_exposed, // GDExtensionBool is_exposed;
		p_extension_funcs->is_runtime, // GDExtensionBool is_runtime;
		nullptr, // GDExtensionConstStringPtr icon_path;
		p_extension_funcs->set_func, // GDExtensionClassSet set_func;
		p_extension_funcs->get_func, // GDExtensionClassGet get_func;
		p_extension_funcs->get_property_list_func, // GDExtensionClassGetPropertyList get_property_list_func;
		p_extension_funcs->free_property_list_func, // GDExtensionClassFreePropertyList2 free_property_list_func;
		p_extension_funcs->property_can_revert_func, // GDExtensionClassPropertyCanRevert property_can_revert_func;
		p_extension_funcs->property_get_revert_func, // GDExtensionClassPropertyGetRevert property_get_revert_func;
		p_extension_funcs->validate_property_func, // GDExtensionClassValidateProperty validate_property_func;
		p_extension_funcs->notification_func, // GDExtensionClassNotification2 notification_func;
		p_extension_funcs->to_string_func, // GDExtensionClassToString to_string_func;
		p_extension_funcs->reference_func, // GDExtensionClassReference reference_func;
		p_extension_funcs->unreference_func, // GDExtensionClassUnreference unreference_func;
		nullptr, // GDExtensionClassCreateInstance3 create_instance_func; /* this one is mandatory */
		p_extension_funcs->free_instance_func, // GDExtensionClassFreeInstance free_instance_func; /* this one is mandatory */
		p_extension_funcs->recreate_instance_func, // GDExtensionClassRecreateInstance recreate_instance_func;
		nullptr, // GDExtensionClassGetVirtual2 get_virtual_func;
		nullptr, // GDExtensionClassGetVirtualCallData2 get_virtual_call_data_func;
		p_extension_funcs->call_virtual_with_data_func, // GDExtensionClassCallVirtualWithData call_virtual_with_data_func;
		p_extension_funcs->class_userdata, // void *class_userdata;
	};

	const ClassCreationDeprecatedInfo legacy = {
		!p_extension_funcs->is_exposed, // bool legacy_unexposed_class;
		nullptr, // GDExtensionClassNotification notification_func;
		nullptr, // GDExtensionClassFreePropertyList free_property_list_func;
		p_extension_funcs->create_instance_func, // GDExtensionClassCreateInstance create_instance_func;
		nullptr, // GDExtensionClassCreateInstance2 create_instance2_func;
		p_extension_funcs->get_rid_func, // GDExtensionClassGetRID get_rid_func;
		p_extension_funcs->get_virtual_func, // GDExtensionClassGetVirtual get_virtual_func;
		p_extension_funcs->get_virtual_call_data_func, // GDExtensionClassGetVirtualCallData get_virtual_call_data_func;
	};
	_register_extension_class_internal(p_library, p_class_name, p_parent_class_name, &class_info6, &legacy);
}

void GDExtension::_register_extension_class4(GDExtensionClassLibraryPtr p_library, GDExtensionConstStringNamePtr p_class_name, GDExtensionConstStringNamePtr p_parent_class_name, const GDExtensionClassCreationInfo4 *p_extension_funcs) {
	const GDExtensionClassCreationInfo6 class_info6 = {
		p_extension_funcs->is_virtual, // GDExtensionBool is_virtual;
		p_extension_funcs->is_abstract, // GDExtensionBool is_abstract;
		p_extension_funcs->is_exposed, // GDExtensionBool is_exposed;
		p_extension_funcs->is_runtime, // GDExtensionBool is_runtime;
		p_extension_funcs->icon_path, // GDExtensionConstStringPtr icon_path;
		p_extension_funcs->set_func, // GDExtensionClassSet set_func;
		p_extension_funcs->get_func, // GDExtensionClassGet get_func;
		p_extension_funcs->get_property_list_func, // GDExtensionClassGetPropertyList get_property_list_func;
		p_extension_funcs->free_property_list_func, // GDExtensionClassFreePropertyList2 free_property_list_func;
		p_extension_funcs->property_can_revert_func, // GDExtensionClassPropertyCanRevert property_can_revert_func;
		p_extension_funcs->property_get_revert_func, // GDExtensionClassPropertyGetRevert property_get_revert_func;
		p_extension_funcs->validate_property_func, // GDExtensionClassValidateProperty validate_property_func;
		p_extension_funcs->notification_func, // GDExtensionClassNotification2 notification_func;
		p_extension_funcs->to_string_func, // GDExtensionClassToString to_string_func;
		p_extension_funcs->reference_func, // GDExtensionClassReference reference_func;
		p_extension_funcs->unreference_func, // GDExtensionClassUnreference unreference_func;
		nullptr, // GDExtensionClassCreateInstance3 create_instance_func; /* this one is mandatory */
		p_extension_funcs->free_instance_func, // GDExtensionClassFreeInstance free_instance_func; /* this one is mandatory */
		p_extension_funcs->recreate_instance_func, // GDExtensionClassRecreateInstance recreate_instance_func;
		p_extension_funcs->get_virtual_func, // GDExtensionClassGetVirtual2 get_virtual_func;
		p_extension_funcs->get_virtual_call_data_func, // GDExtensionClassGetVirtualCallData2 get_virtual_call_data_func;
		p_extension_funcs->call_virtual_with_data_func, // GDExtensionClassCallVirtualWithData call_virtual_with_data_func;
		p_extension_funcs->class_userdata, // void *class_userdata;
	};

	const ClassCreationDeprecatedInfo legacy = {
		!p_extension_funcs->is_exposed, // bool legacy_unexposed_class;
		nullptr, // GDExtensionClassNotification notification_func;
		nullptr, // GDExtensionClassFreePropertyList free_property_list_func;
		nullptr, // GDExtensionClassCreateInstance create_instance_func;
		p_extension_funcs->create_instance_func, // GDExtensionClassCreateInstance2 create_instance2_func;
		nullptr, // GDExtensionClassGetRID get_rid_func;
		nullptr, // GDExtensionClassGetVirtual get_virtual_func;
		nullptr, // GDExtensionClassGetVirtualCallData get_virtual_call_data_func;
	};
	_register_extension_class_internal(p_library, p_class_name, p_parent_class_name, &class_info6, &legacy);
}

void GDExtension::_register_extension_class5(GDExtensionClassLibraryPtr p_library, GDExtensionConstStringNamePtr p_class_name, GDExtensionConstStringNamePtr p_parent_class_name, const GDExtensionClassCreationInfo5 *p_extension_funcs) {
	const GDExtensionClassCreationInfo6 class_info6 = {
		p_extension_funcs->is_virtual, // GDExtensionBool is_virtual;
		p_extension_funcs->is_abstract, // GDExtensionBool is_abstract;
		p_extension_funcs->is_exposed, // GDExtensionBool is_exposed;
		p_extension_funcs->is_runtime, // GDExtensionBool is_runtime;
		p_extension_funcs->icon_path, // GDExtensionConstStringPtr icon_path;
		p_extension_funcs->set_func, // GDExtensionClassSet set_func;
		p_extension_funcs->get_func, // GDExtensionClassGet get_func;
		p_extension_funcs->get_property_list_func, // GDExtensionClassGetPropertyList get_property_list_func;
		p_extension_funcs->free_property_list_func, // GDExtensionClassFreePropertyList2 free_property_list_func;
		p_extension_funcs->property_can_revert_func, // GDExtensionClassPropertyCanRevert property_can_revert_func;
		p_extension_funcs->property_get_revert_func, // GDExtensionClassPropertyGetRevert property_get_revert_func;
		p_extension_funcs->validate_property_func, // GDExtensionClassValidateProperty validate_property_func;
		p_extension_funcs->notification_func, // GDExtensionClassNotification2 notification_func;
		p_extension_funcs->to_string_func, // GDExtensionClassToString to_string_func;
		p_extension_funcs->reference_func, // GDExtensionClassReference reference_func;
		p_extension_funcs->unreference_func, // GDExtensionClassUnreference unreference_func;
		nullptr, // GDExtensionClassCreateInstance3 create_instance_func; /* this one is mandatory */
		p_extension_funcs->free_instance_func, // GDExtensionClassFreeInstance free_instance_func; /* this one is mandatory */
		p_extension_funcs->recreate_instance_func, // GDExtensionClassRecreateInstance recreate_instance_func;
		p_extension_funcs->get_virtual_func, // GDExtensionClassGetVirtual2 get_virtual_func;
		p_extension_funcs->get_virtual_call_data_func, // GDExtensionClassGetVirtualCallData2 get_virtual_call_data_func;
		p_extension_funcs->call_virtual_with_data_func, // GDExtensionClassCallVirtualWithData call_virtual_with_data_func;
		p_extension_funcs->class_userdata, // void *class_userdata;
	};

	const ClassCreationDeprecatedInfo legacy = {
		false, // bool legacy_unexposed_class;
		nullptr, // GDExtensionClassNotification notification_func;
		nullptr, // GDExtensionClassFreePropertyList free_property_list_func;
		nullptr, // GDExtensionClassCreateInstance create_instance_func;
		p_extension_funcs->create_instance_func, // GDExtensionClassCreateInstance2 create_instance2_func;
		nullptr, // GDExtensionClassGetRID get_rid_func;
		nullptr, // GDExtensionClassGetVirtual get_virtual_func;
		nullptr, // GDExtensionClassGetVirtualCallData get_virtual_call_data_func;
	};
	_register_extension_class_internal(p_library, p_class_name, p_parent_class_name, &class_info6, &legacy);
}
#endif // DISABLE_DEPRECATED

void GDExtension::_register_extension_class6(GDExtensionClassLibraryPtr p_library, GDExtensionConstStringNamePtr p_class_name, GDExtensionConstStringNamePtr p_parent_class_name, const GDExtensionClassCreationInfo6 *p_extension_funcs) {
	_register_extension_class_internal(p_library, p_class_name, p_parent_class_name, p_extension_funcs);
}

void GDExtension::_register_extension_class_internal(GDExtensionClassLibraryPtr p_library, GDExtensionConstStringNamePtr p_class_name, GDExtensionConstStringNamePtr p_parent_class_name, const GDExtensionClassCreationInfo6 *p_extension_funcs, const ClassCreationDeprecatedInfo *p_deprecated_funcs) {
	GDExtension *self = reinterpret_cast<GDExtension *>(p_library);

	GDExtensionDeviceExit::CallbackScope registration(self->library_exit_ref);
	ERR_FAIL_COND(registration.get_error() != OK);
	uint64_t entry_classes = (1u << 5) | (1u << 7); // construction and destruction
	if (p_extension_funcs->set_func || p_extension_funcs->get_func || p_extension_funcs->get_property_list_func || p_extension_funcs->free_property_list_func || p_extension_funcs->property_can_revert_func || p_extension_funcs->property_get_revert_func || p_extension_funcs->validate_property_func || p_extension_funcs->to_string_func) { entry_classes |= 1u << 2; }
	if (p_extension_funcs->notification_func) { entry_classes |= 1u << 3; }
	if (p_extension_funcs->get_virtual_func || p_extension_funcs->get_virtual_call_data_func || p_extension_funcs->call_virtual_with_data_func) { entry_classes |= 1u << 4; }
	if (p_extension_funcs->reference_func || p_extension_funcs->unreference_func) { entry_classes |= 1u << 9; }
#ifndef DISABLE_DEPRECATED
	if (p_deprecated_funcs) {
		if (p_deprecated_funcs->notification_func) { entry_classes |= 1u << 3; }
		if (p_deprecated_funcs->free_property_list_func || p_deprecated_funcs->get_rid_func) { entry_classes |= 1u << 2; }
		if (p_deprecated_funcs->get_virtual_func || p_deprecated_funcs->get_virtual_call_data_func) { entry_classes |= 1u << 4; }
	}
#endif
	const StringName &registered_parent = *reinterpret_cast<const StringName *>(p_parent_class_name);
	if (registered_parent == StringName("MainLoop") || ClassDB::is_parent_class(registered_parent, "MainLoop")) { entry_classes |= 1u << 1; }
	ERR_FAIL_COND_MSG(GDExtensionDeviceExit::Registry::get_singleton().check_entry_classes(self->library_exit_ref, entry_classes) != OK, "UNSUPPORTED_ENTRY_CLASS");
	GDExtensionDeviceExit::RecordRef class_reference;
	ERR_FAIL_COND(GDExtensionDeviceExit::Registry::get_singleton().retain_private(self->library_exit_ref, class_reference) != OK);

	StringName class_name = *reinterpret_cast<const StringName *>(p_class_name);
	StringName parent_class_name = *reinterpret_cast<const StringName *>(p_parent_class_name);
	ERR_FAIL_COND_MSG(!String(class_name).is_valid_unicode_identifier(), vformat("Attempt to register extension class '%s', which is not a valid class identifier.", class_name));
	ERR_FAIL_COND_MSG(ClassDB::class_exists(class_name), vformat("Attempt to register extension class '%s', which appears to be already registered.", class_name));

	Extension *parent_extension = nullptr;

	if (self->extension_classes.has(parent_class_name)) {
		parent_extension = &self->extension_classes[parent_class_name];
	} else if (ClassDB::class_exists(parent_class_name)) {
		if (ClassDB::get_api_type(parent_class_name) == ClassDB::API_EXTENSION || ClassDB::get_api_type(parent_class_name) == ClassDB::API_EDITOR_EXTENSION) {
			ERR_PRINT("Unimplemented yet");
			//inheriting from another extension
		} else {
			//inheriting from engine class
		}
	} else {
		ERR_FAIL_MSG(vformat("Attempt to register an extension class '%s' using non-existing parent class '%s'.", String(class_name), String(parent_class_name)));
	}

#ifdef TOOLS_ENABLED
	Extension *extension = nullptr;
	bool is_runtime = (bool)p_extension_funcs->is_runtime;
	if (self->is_reloading && self->extension_classes.has(class_name)) {
		extension = &self->extension_classes[class_name];
		if (!parent_extension && parent_class_name != extension->gdextension.parent_class_name) {
			ERR_FAIL_MSG(vformat("GDExtension class '%s' cannot change parent type from '%s' to '%s' on hot reload. Restart Godot for this change to take effect.", class_name, extension->gdextension.parent_class_name, parent_class_name));
		}
		if (extension->gdextension.is_runtime != is_runtime) {
			ERR_PRINT(vformat("GDExtension class '%s' cannot change to/from runtime class on hot reload. Restart Godot for this change to take effect.", class_name));
			is_runtime = extension->gdextension.is_runtime;
		}
		extension->is_reloading = false;
	} else {
		self->extension_classes[class_name] = Extension();
		extension = &self->extension_classes[class_name];
	}
#else
	self->extension_classes[class_name] = Extension();
	Extension *extension = &self->extension_classes[class_name];
#endif

	if (parent_extension) {
		extension->gdextension.parent = &parent_extension->gdextension;
		parent_extension->gdextension.children.push_back(&extension->gdextension);
	}

	if (self->reloadable && p_extension_funcs->recreate_instance_func == nullptr) {
		bool can_create_class = (bool)p_extension_funcs->create_instance_func;
#ifndef DISABLE_DEPRECATED
		if (!can_create_class && p_deprecated_funcs) {
			can_create_class = (bool)p_deprecated_funcs->create_instance_func;
		}
#endif
		if (can_create_class) {
			ERR_PRINT(vformat("Extension marked as reloadable, but attempted to register class '%s' which doesn't support reloading. Perhaps your language binding don't support it? Reloading disabled for this extension.", class_name));
			self->reloadable = false;
		}
	}

	extension->gdextension.callback_record = GDExtensionDeviceExit::SharedRecord(std::move(class_reference));
	extension->gdextension.library = self;
	extension->gdextension.parent_class_name = parent_class_name;
	extension->gdextension.class_name = class_name;
	extension->gdextension.editor_class = self->level_initialized == INITIALIZATION_LEVEL_EDITOR;
	extension->gdextension.is_virtual = p_extension_funcs->is_virtual;
	extension->gdextension.is_abstract = p_extension_funcs->is_abstract;
	extension->gdextension.is_exposed = p_extension_funcs->is_exposed;
#ifdef TOOLS_ENABLED
	extension->gdextension.is_runtime = is_runtime;
#endif
	extension->gdextension.set = p_extension_funcs->set_func;
	extension->gdextension.get = p_extension_funcs->get_func;
	extension->gdextension.get_property_list = p_extension_funcs->get_property_list_func;
	extension->gdextension.free_property_list2 = p_extension_funcs->free_property_list_func;
	extension->gdextension.property_can_revert = p_extension_funcs->property_can_revert_func;
	extension->gdextension.property_get_revert = p_extension_funcs->property_get_revert_func;
	extension->gdextension.validate_property = p_extension_funcs->validate_property_func;
#ifndef DISABLE_DEPRECATED
	if (p_deprecated_funcs) {
		extension->gdextension.legacy_unexposed_class = p_deprecated_funcs->legacy_unexposed_class;
		extension->gdextension.notification = p_deprecated_funcs->notification_func;
		extension->gdextension.free_property_list = p_deprecated_funcs->free_property_list_func;
		extension->gdextension.create_instance = p_deprecated_funcs->create_instance_func;
		extension->gdextension.create_instance2 = p_deprecated_funcs->create_instance2_func;
		extension->gdextension.get_rid = p_deprecated_funcs->get_rid_func;
		extension->gdextension.get_virtual = p_deprecated_funcs->get_virtual_func;
		extension->gdextension.get_virtual_call_data = p_deprecated_funcs->get_virtual_call_data_func;
	}
#endif // DISABLE_DEPRECATED
	extension->gdextension.notification2 = p_extension_funcs->notification_func;
	extension->gdextension.to_string = p_extension_funcs->to_string_func;
	extension->gdextension.reference = p_extension_funcs->reference_func;
	extension->gdextension.unreference = p_extension_funcs->unreference_func;
	extension->gdextension.class_userdata = p_extension_funcs->class_userdata;
	extension->gdextension.create_instance3 = p_extension_funcs->create_instance_func;
	extension->gdextension.free_instance = p_extension_funcs->free_instance_func;
	extension->gdextension.recreate_instance = p_extension_funcs->recreate_instance_func;
	extension->gdextension.get_virtual2 = p_extension_funcs->get_virtual_func;
	extension->gdextension.get_virtual_call_data2 = p_extension_funcs->get_virtual_call_data_func;
	extension->gdextension.call_virtual_with_data = p_extension_funcs->call_virtual_with_data_func;

	extension->gdextension.reloadable = self->reloadable;
#ifdef TOOLS_ENABLED
	if (extension->gdextension.reloadable) {
		extension->gdextension.tracking_userdata = extension;
		extension->gdextension.track_instance = &GDExtension::_track_instance;
		extension->gdextension.untrack_instance = &GDExtension::_untrack_instance;
	} else {
		extension->gdextension.tracking_userdata = nullptr;
		extension->gdextension.track_instance = nullptr;
		extension->gdextension.untrack_instance = nullptr;
	}
#endif

	extension->gdextension.create_gdtype();

	ClassDB::register_extension_class(&extension->gdextension);

	if (p_extension_funcs->icon_path != nullptr) {
		const String icon_path = *reinterpret_cast<const String *>(p_extension_funcs->icon_path);
		if (!icon_path.is_empty()) {
			self->class_icon_paths[class_name] = icon_path;
		}
	}
}

void GDExtension::_register_extension_class_method(GDExtensionClassLibraryPtr p_library, GDExtensionConstStringNamePtr p_class_name, const GDExtensionClassMethodInfo *p_method_info) {
	GDExtension *self = reinterpret_cast<GDExtension *>(p_library);
	GDExtensionDeviceExit::CallbackScope callback_scope(self->library_exit_ref);
	ERR_FAIL_COND_MSG(callback_scope.get_error() != OK, "ENTRY_CLOSED");
	ERR_FAIL_COND_MSG(GDExtensionDeviceExit::Registry::get_singleton().check_entry_classes(self->library_exit_ref, 1u << 0) != OK, "UNSUPPORTED_ENTRY_CLASS");
	if (p_method_info->has_return_value) {
		if (!p_method_info->return_value_info) {
			ERR_FAIL_MSG("UNSUPPORTED_ENTRY_CLASS");
		}
		const Variant::Type return_type = Variant::Type(p_method_info->return_value_info->type);
		const auto metadata = p_method_info->return_value_metadata;
		const bool int_metadata = metadata == GDEXTENSION_METHOD_ARGUMENT_METADATA_NONE ||
				(metadata >= GDEXTENSION_METHOD_ARGUMENT_METADATA_INT_IS_INT8 && metadata <= GDEXTENSION_METHOD_ARGUMENT_METADATA_INT_IS_UINT64) ||
				metadata == GDEXTENSION_METHOD_ARGUMENT_METADATA_INT_IS_CHAR16 || metadata == GDEXTENSION_METHOD_ARGUMENT_METADATA_INT_IS_CHAR32;
		const bool real_metadata = metadata == GDEXTENSION_METHOD_ARGUMENT_METADATA_NONE ||
				metadata == GDEXTENSION_METHOD_ARGUMENT_METADATA_REAL_IS_FLOAT || metadata == GDEXTENSION_METHOD_ARGUMENT_METADATA_REAL_IS_DOUBLE;
		const bool object_metadata = metadata == GDEXTENSION_METHOD_ARGUMENT_METADATA_NONE || metadata == GDEXTENSION_METHOD_ARGUMENT_METADATA_OBJECT_IS_REQUIRED;
		const bool metadata_valid = return_type == Variant::INT ? int_metadata : return_type == Variant::FLOAT ? real_metadata
				: return_type == Variant::OBJECT															   ? object_metadata
																											   : metadata == GDEXTENSION_METHOD_ARGUMENT_METADATA_NONE;
		if (return_type < Variant::NIL || return_type >= Variant::VARIANT_MAX || !metadata_valid || (return_type == Variant::OBJECT && (p_method_info->return_value_info->class_name == nullptr || !ClassDB::class_exists(*reinterpret_cast<const StringName *>(p_method_info->return_value_info->class_name))))) {
			ERR_FAIL_MSG("UNSUPPORTED_ENTRY_CLASS");
		}
	}
	GDExtensionDeviceExit::RecordRef method_reference;
	ERR_FAIL_COND(GDExtensionDeviceExit::Registry::get_singleton().retain_private(self->library_exit_ref, method_reference) != OK);

	StringName class_name = *reinterpret_cast<const StringName *>(p_class_name);
	StringName method_name = *reinterpret_cast<const StringName *>(p_method_info->name);
	ERR_FAIL_COND_MSG(!self->extension_classes.has(class_name), vformat("Attempt to register extension method '%s' for unexisting class '%s'.", String(method_name), class_name));

#ifdef TOOLS_ENABLED
	Extension *extension = &self->extension_classes[class_name];
	GDExtensionMethodBind *method = nullptr;

	// If the extension is still marked as reloading, that means it failed to register again.
	if (extension->is_reloading) {
		return;
	}

	if (self->is_reloading && extension->methods.has(method_name)) {
		method = extension->methods[method_name];

		// Try to update the method bind. If it doesn't work (because it's incompatible) then
		// mark as invalid and create a new one.
		if (!method->is_reloading || !method->try_update(p_method_info, method_reference)) {
			method->valid = false;
			self->invalid_methods.push_back(method);

			method = nullptr;
		}
	}

	if (method == nullptr) {
		method = memnew(GDExtensionMethodBind(p_method_info, std::move(method_reference)));
		method->set_instance_class(class_name);
		extension->methods[method_name] = method;
	} else {
		method->is_reloading = false;
	}
#else
	GDExtensionMethodBind *method = memnew(GDExtensionMethodBind(p_method_info, std::move(method_reference)));
	method->set_instance_class(class_name);
#endif

	ClassDB::bind_method_custom(class_name, method);
}

void GDExtension::_register_extension_class_virtual_method(GDExtensionClassLibraryPtr p_library, GDExtensionConstStringNamePtr p_class_name, const GDExtensionClassVirtualMethodInfo *p_method_info) {
	StringName class_name = *reinterpret_cast<const StringName *>(p_class_name);
	ClassDB::add_extension_class_virtual_method(class_name, p_method_info);
}

void GDExtension::_register_extension_class_integer_constant(GDExtensionClassLibraryPtr p_library, GDExtensionConstStringNamePtr p_class_name, GDExtensionConstStringNamePtr p_enum_name, GDExtensionConstStringNamePtr p_constant_name, GDExtensionInt p_constant_value, GDExtensionBool p_is_bitfield) {
	GDExtension *self = reinterpret_cast<GDExtension *>(p_library);

	StringName class_name = *reinterpret_cast<const StringName *>(p_class_name);
	StringName enum_name = *reinterpret_cast<const StringName *>(p_enum_name);
	StringName constant_name = *reinterpret_cast<const StringName *>(p_constant_name);
	ERR_FAIL_COND_MSG(!self->extension_classes.has(class_name), vformat("Attempt to register extension constant '%s' for unexisting class '%s'.", constant_name, class_name));

#ifdef TOOLS_ENABLED
	// If the extension is still marked as reloading, that means it failed to register again.
	Extension *extension = &self->extension_classes[class_name];
	if (extension->is_reloading) {
		return;
	}
#endif

	ClassDB::bind_integer_constant(class_name, enum_name, constant_name, p_constant_value, p_is_bitfield);
}

void GDExtension::_register_extension_class_property(GDExtensionClassLibraryPtr p_library, GDExtensionConstStringNamePtr p_class_name, const GDExtensionPropertyInfo *p_info, GDExtensionConstStringNamePtr p_setter, GDExtensionConstStringNamePtr p_getter) {
	_register_extension_class_property_indexed(p_library, p_class_name, p_info, p_setter, p_getter, -1);
}

void GDExtension::_register_extension_class_property_indexed(GDExtensionClassLibraryPtr p_library, GDExtensionConstStringNamePtr p_class_name, const GDExtensionPropertyInfo *p_info, GDExtensionConstStringNamePtr p_setter, GDExtensionConstStringNamePtr p_getter, GDExtensionInt p_index) {
	GDExtension *self = reinterpret_cast<GDExtension *>(p_library);

	StringName class_name = *reinterpret_cast<const StringName *>(p_class_name);
	StringName setter = *reinterpret_cast<const StringName *>(p_setter);
	StringName getter = *reinterpret_cast<const StringName *>(p_getter);
	String property_name = *reinterpret_cast<const StringName *>(p_info->name);
	ERR_FAIL_COND_MSG(!self->extension_classes.has(class_name), vformat("Attempt to register extension class property '%s' for unexisting class '%s'.", property_name, class_name));

#ifdef TOOLS_ENABLED
	// If the extension is still marked as reloading, that means it failed to register again.
	Extension *extension = &self->extension_classes[class_name];
	if (extension->is_reloading) {
		return;
	}
#endif

	PropertyInfo pinfo(*p_info);

	ClassDB::add_property(class_name, pinfo, setter, getter, p_index);
}

void GDExtension::_register_extension_class_property_group(GDExtensionClassLibraryPtr p_library, GDExtensionConstStringNamePtr p_class_name, GDExtensionConstStringPtr p_group_name, GDExtensionConstStringPtr p_prefix) {
	GDExtension *self = reinterpret_cast<GDExtension *>(p_library);

	StringName class_name = *reinterpret_cast<const StringName *>(p_class_name);
	String group_name = *reinterpret_cast<const String *>(p_group_name);
	String prefix = *reinterpret_cast<const String *>(p_prefix);
	ERR_FAIL_COND_MSG(!self->extension_classes.has(class_name), vformat("Attempt to register extension class property group '%s' for unexisting class '%s'.", group_name, class_name));

#ifdef TOOLS_ENABLED
	// If the extension is still marked as reloading, that means it failed to register again.
	Extension *extension = &self->extension_classes[class_name];
	if (extension->is_reloading) {
		return;
	}
#endif

	ClassDB::add_property_group(class_name, group_name, prefix);
}

void GDExtension::_register_extension_class_property_subgroup(GDExtensionClassLibraryPtr p_library, GDExtensionConstStringNamePtr p_class_name, GDExtensionConstStringPtr p_subgroup_name, GDExtensionConstStringPtr p_prefix) {
	GDExtension *self = reinterpret_cast<GDExtension *>(p_library);

	StringName class_name = *reinterpret_cast<const StringName *>(p_class_name);
	String subgroup_name = *reinterpret_cast<const String *>(p_subgroup_name);
	String prefix = *reinterpret_cast<const String *>(p_prefix);
	ERR_FAIL_COND_MSG(!self->extension_classes.has(class_name), vformat("Attempt to register extension class property subgroup '%s' for unexisting class '%s'.", subgroup_name, class_name));

#ifdef TOOLS_ENABLED
	// If the extension is still marked as reloading, that means it failed to register again.
	Extension *extension = &self->extension_classes[class_name];
	if (extension->is_reloading) {
		return;
	}
#endif

	ClassDB::add_property_subgroup(class_name, subgroup_name, prefix);
}

void GDExtension::_register_extension_class_signal(GDExtensionClassLibraryPtr p_library, GDExtensionConstStringNamePtr p_class_name, GDExtensionConstStringNamePtr p_signal_name, const GDExtensionPropertyInfo *p_argument_info, GDExtensionInt p_argument_count) {
	GDExtension *self = reinterpret_cast<GDExtension *>(p_library);

	StringName class_name = *reinterpret_cast<const StringName *>(p_class_name);
	StringName signal_name = *reinterpret_cast<const StringName *>(p_signal_name);
	ERR_FAIL_COND_MSG(!self->extension_classes.has(class_name), vformat("Attempt to register extension class signal '%s' for unexisting class '%s'.", signal_name, class_name));

#ifdef TOOLS_ENABLED
	// If the extension is still marked as reloading, that means it failed to register again.
	Extension *extension = &self->extension_classes[class_name];
	if (extension->is_reloading) {
		return;
	}
#endif

	MethodInfo s;
	s.name = signal_name;
	for (int i = 0; i < p_argument_count; i++) {
		PropertyInfo arg(p_argument_info[i]);
		s.arguments.push_back(arg);
	}
	ClassDB::add_signal(class_name, s);
}

void GDExtension::_unregister_extension_class(GDExtensionClassLibraryPtr p_library, GDExtensionConstStringNamePtr p_class_name) {
	GDExtension *self = reinterpret_cast<GDExtension *>(p_library);

	StringName class_name = *reinterpret_cast<const StringName *>(p_class_name);
	ERR_FAIL_COND_MSG(!self->extension_classes.has(class_name), vformat("Attempt to unregister unexisting extension class '%s'.", class_name));

	Extension *ext = &self->extension_classes[class_name];
#ifdef TOOLS_ENABLED
	if (self->scene_ready_aborting) {
		ERR_FAIL_COND_MSG(ext->gdextension.children.size(), vformat("Attempt to unregister class '%s' while other extension classes inherit from it.", class_name));
		if (ext->gdextension.parent) { ext->gdextension.parent->children.erase(&ext->gdextension); }
	}
	if (ext->is_reloading || self->scene_ready_aborting) {
		self->_clear_extension(ext);
	}
#endif
	ERR_FAIL_COND_MSG(ext->gdextension.children.size(), vformat("Attempt to unregister class '%s' while other extension classes inherit from it.", class_name));

#ifdef TOOLS_ENABLED
	ClassDB::unregister_extension_class(class_name, !ext->is_reloading && !self->scene_ready_aborting);
#else
	ClassDB::unregister_extension_class(class_name);
#endif

	if (ext->gdextension.parent != nullptr) {
		ext->gdextension.parent->children.erase(&ext->gdextension);
	}

#ifdef TOOLS_ENABLED
	if (!ext->is_reloading && !self->scene_ready_aborting) {
		self->extension_classes.erase(class_name);
	}

	GDExtensionEditorHelp::remove_class(class_name);
#else
	self->extension_classes.erase(class_name);
#endif
}

void GDExtension::_get_library_path(GDExtensionClassLibraryPtr p_library, GDExtensionUninitializedStringPtr r_path) {
	GDExtension *self = reinterpret_cast<GDExtension *>(p_library);

	Ref<GDExtensionLibraryLoader> library_loader = self->loader;
	String library_path;
	if (library_loader.is_valid()) {
		library_path = library_loader->configuration.library_path;
	}

	memnew_placement(r_path, String(library_path));
}

void GDExtension::_register_get_classes_used_callback(GDExtensionClassLibraryPtr p_library, GDExtensionEditorGetClassesUsedCallback p_callback) {
#ifdef TOOLS_ENABLED
	GDExtension *self = reinterpret_cast<GDExtension *>(p_library);
	GDExtensionDeviceExit::CallbackScope callback_scope(self->library_exit_ref);
	ERR_FAIL_COND(callback_scope.get_error() != OK);
	self->get_classes_used_callback = p_callback;
	self->has_get_classes_used_callback = p_callback != nullptr;
#endif
}

void GDExtension::_register_main_loop_callbacks(GDExtensionClassLibraryPtr p_library, const GDExtensionMainLoopCallbacks *p_callbacks) {
	GDExtension *self = reinterpret_cast<GDExtension *>(p_library);
	GDExtensionDeviceExit::CallbackScope callback_scope(self->library_exit_ref);
	ERR_FAIL_COND(callback_scope.get_error() != OK);
	ERR_FAIL_COND_MSG(GDExtensionDeviceExit::Registry::get_singleton().check_entry_classes(self->library_exit_ref, uint64_t(1) << 1) != OK, "UNSUPPORTED_ENTRY_CLASS");
	self->startup_callback = p_callbacks->startup_func;
	self->shutdown_callback = p_callbacks->shutdown_func;
	self->frame_callback = p_callbacks->frame_func;
	self->main_loop_callback_mask =
			(self->startup_callback ? (1u << int(MainLoopCallback::STARTUP)) : 0) |
			(self->shutdown_callback ? (1u << int(MainLoopCallback::SHUTDOWN)) : 0) |
			(self->frame_callback ? (1u << int(MainLoopCallback::FRAME)) : 0);
}

void GDExtension::_call_main_loop_callback(MainLoopCallback p_kind) {
	// An empty core registration is still a no-op after failed reload. Inspect
	// only presence here; read the actual DSO pointer after frame admission.
	if ((main_loop_callback_mask & (1u << int(p_kind))) == 0) {
		return;
	}
	GDExtensionDeviceExit::CallbackScope callback_scope(library_exit_ref);
	ERR_FAIL_COND(callback_scope.get_error() != OK);
	// Shutdown is counted here as an actual callback frame, not certified as a
	// finite cleanup action. The complete K03 cleanup admission remains required.
	void (*callback)() = nullptr;
	switch (p_kind) {
		case MainLoopCallback::STARTUP:
			callback = startup_callback;
			break;
		case MainLoopCallback::SHUTDOWN:
			callback = shutdown_callback;
			break;
		case MainLoopCallback::FRAME:
			callback = frame_callback;
			break;
	}
	if (callback) {
		callback();
		if (p_kind == MainLoopCallback::SHUTDOWN) {
			shutdown_callback_dispatched = true;
		}
	}
}

void GDExtension::register_interface_function(const StringName &p_function_name, GDExtensionInterfaceFunctionPtr p_function_pointer) {
	ERR_FAIL_COND_MSG(gdextension_interface_functions.has(p_function_name), vformat("Attempt to register interface function '%s', which appears to be already registered.", p_function_name));
	gdextension_interface_functions.insert(p_function_name, p_function_pointer);
}

GDExtensionInterfaceFunctionPtr GDExtension::get_interface_function(const StringName &p_function_name) {
	GDExtensionInterfaceFunctionPtr *function = gdextension_interface_functions.getptr(p_function_name);
	ERR_FAIL_NULL_V_MSG(function, nullptr, vformat("Attempt to get non-existent interface function: '%s'.", String(p_function_name)));
	return *function;
}

Error GDExtension::open_library(const String &p_path, const Ref<GDExtensionLoader> &p_loader) {
	if (library_close_error != OK) {
		return library_close_error;
	}
	ERR_FAIL_COND_V_MSG(library_opening || is_library_open(), ERR_BUSY, "Can't replace an open GDExtension library.");
	ERR_FAIL_COND_V_MSG(p_loader.is_null(), FAILED, "Can't open GDExtension without a loader.");
	Ref<GDExtension> self(this);
	Ref<GDExtensionLoader> candidate = p_loader;
	library_opening = true;
	struct OpeningScope {
		bool &active;
		~OpeningScope() { active = false; }
	} opening_scope{ library_opening };
	Error err = candidate->prepare_open(p_path);
	if (err != OK) {
		return err;
	}
	struct PreparedOpen {
		GDExtensionLoader *owner;
		~PreparedOpen() {
			if (owner) {
				owner->abort_prepared_open();
			}
		}
	} prepared{ candidate.ptr() };
	const uint32_t guard_version = candidate->get_device_exit_guard_version();
	if (guard_version > 1) {
		return ERR_INVALID_PARAMETER;
	}
	auto &registry = GDExtensionDeviceExit::Registry::get_singleton();
	GDExtensionDeviceExit::RecordRef wrapper_ref;
	GDExtensionDeviceExit::RecordRef scope_ref;
	err = registry.reserve_open(uint32_t(candidate->get_loader_kind()), wrapper_ref, scope_ref, this);
	if (err != OK) {
		return err;
	}
	loader = candidate;
	prepared.owner = nullptr; // Built-in open owns every consume/failure exit from here.
	err = loader->open_library(p_path);
	ERR_FAIL_COND_V_MSG(err == ERR_FILE_NOT_FOUND, err, vformat("GDExtension dynamic library not found: '%s'.", p_path));
	ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Can't open GDExtension dynamic library: '%s'.", p_path));

	library_exit_ref = std::move(wrapper_ref);
	scene_ready_load_error = OK;
	library_exit_route = GDExtensionDeviceExit::CloseRoute::LAST_REF;
	library_exit_route_selected = false;
	registry.attach(library_exit_ref);
	{
		GDExtensionDeviceExit::BootstrapScope scope(std::move(scope_ref), this);
		err = loader->initialize(guard_version == 1 ? &gdextension_get_guarded_proc_address : &gdextension_get_proc_address, self, &initialization);
		registry.observe_initialize(scope.reference, err);
		err = scope.observe_tuple(initialization, err);
		if (err == OK && guard_version == 1 && !registry.guard_qualified(scope.reference)) {
			// A version-one entry that skipped registration has no guarded
			// teardown authority. The original bootstrap still owns this abort.
			err = ERR_UNAVAILABLE;
		}
	}
	library_opening = false; // Actual entry returned; init-abort cleanup may now close.
	if (err != OK) {
		// Errors already logged in initialize(). Retained views keep the actual outcome.
		GDExtensionDeviceExit::require_close_success(_close_library(GDExtensionDeviceExit::CloseRoute::INIT_ABORT), GDExtensionDeviceExit::CloseRoute::INIT_ABORT);
		return err;
	}
	level_initialized = -1;
	return OK;
}

Error GDExtension::preflight_exit(GDExtensionDeviceExit::CloseRoute p_route) {
	if (library_close_error != OK) {
		return library_close_error;
	}
	if (library_opening || library_closing) {
		return ERR_BUSY;
	}
	if (!is_library_open()) {
		return ERR_UNCONFIGURED;
	}
	// Initialized teardown and main-loop shutdown have no typed issuer in
	// this build. Refuse them before sealing or touching the image.
	auto &registry = GDExtensionDeviceExit::Registry::get_singleton();
	if (registry.guard_qualified(library_exit_ref)) {
		if (shutdown_callback && !shutdown_callback_dispatched) { return ERR_UNAVAILABLE; } // Control census not activated.
		const Error result = registry.guarded_preflight(library_exit_ref, p_route);
		if (result == OK || result == ERR_BUSY) { library_exit_route = p_route; library_exit_route_selected = true; }
		return result;
	}
	if (level_initialized >= 0 || shutdown_callback) {
		return ERR_UNAVAILABLE;
	}
	// An active bootstrap/ordinary frame seals normal admission and returns
	// Busy; the same attempt can be retried after that frame returns.
	const Error result = registry.preflight_exit(library_exit_ref, p_route);
	if (result == OK || result == ERR_BUSY) { library_exit_route = p_route; library_exit_route_selected = true; }
	return result;
}

Error GDExtension::close_library() {
	return _close_library(GDExtensionDeviceExit::CloseRoute::EXPLICIT);
}

Error GDExtension::_close_library(GDExtensionDeviceExit::CloseRoute p_route) {
	if (library_close_error != OK) {
		return library_close_error;
	}
	if (library_closing || library_opening) {
		return ERR_BUSY;
	}
	const Error preflight = preflight_exit(p_route);
	if (preflight != OK) {
		return preflight;
	}
	library_closing = true;
	struct ClosingScope {
		bool &active;
		~ClosingScope() { active = false; }
	} closing_scope{ library_closing };
	ERR_FAIL_COND_V(!is_library_open(), ERR_UNCONFIGURED);
	// Seal normal callback/publication admission before asking the loader to
	// close. A Busy observation is sticky: active frames/holders must retire,
	// then the caller may retry this same close operation.
	const Error published = GDExtensionDeviceExit::Registry::get_singleton().prepare_selected_preclose(library_exit_ref, p_route);
	if (published != OK) {
		return published;
	}
	const Error admitted = GDExtensionDeviceExit::Registry::get_singleton().begin_close(library_exit_ref, p_route);
	if (admitted != OK) {
		return admitted;
	}
	{
		GDExtensionDeviceExit::NativeCloseScope native_close(library_exit_ref, loader.ptr());
		library_close_error = loader->close_library();
	}
	GDExtensionDeviceExit::Registry::get_singleton().observe_close(library_exit_ref, library_close_error);
	if (library_close_error != OK) {
		return library_close_error;
	}
	GDExtensionDeviceExit::Registry::get_singleton().detach(library_exit_ref);
	library_exit_ref.reset();

	class_icon_paths.clear();

#ifdef TOOLS_ENABLED
	instance_bindings.clear();
#endif
	return OK;
}

bool GDExtension::is_library_open() const {
	if (library_close_error != OK) {
		return true; // Failed association retained; do not call the failed loader.
	}
	return loader.is_valid() && loader->is_library_open();
}

GDExtension::InitializationLevel GDExtension::get_minimum_library_initialization_level() const {
	ERR_FAIL_COND_V(library_close_error != OK, INITIALIZATION_LEVEL_CORE);
	ERR_FAIL_COND_V(!is_library_open(), INITIALIZATION_LEVEL_CORE);
	return InitializationLevel(initialization.minimum_initialization_level);
}

Error GDExtension::initialize_library(InitializationLevel p_level) {
	if (library_close_error != OK) {
		return library_close_error;
	}
	ERR_FAIL_COND_V(!is_library_open(), ERR_UNCONFIGURED);
	ERR_FAIL_COND_V(p_level < INITIALIZATION_LEVEL_CORE || p_level > INITIALIZATION_LEVEL_EDITOR, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V_MSG(p_level <= int32_t(level_initialized), ERR_INVALID_PARAMETER, vformat("Level '%d' must be higher than the current level '%d'", p_level, level_initialized));
	ERR_FAIL_NULL_V(initialization.initialize, ERR_UNCONFIGURED);

	{
		GDExtensionDeviceExit::LevelScope callback_scope(library_exit_ref, this, initialization, int32_t(p_level), true);
		ERR_FAIL_COND_V(callback_scope.get_error() != OK, callback_scope.get_error());
		level_initialized = int32_t(p_level);
		initialization.initialize(initialization.userdata, GDExtensionInitializationLevel(p_level));
	}
	if (p_level == INITIALIZATION_LEVEL_SCENE) {
		scene_ready_load_error = GDExtensionDeviceExit::Registry::get_singleton().scene_ready_initialization_result(library_exit_ref);
		return scene_ready_load_error;
	}
	return OK; // Dispatch completion only; the legacy callback has no typed G2 result.
}
Error GDExtension::deinitialize_library(InitializationLevel p_level) {
	if (library_close_error != OK) {
		return library_close_error;
	}
	ERR_FAIL_COND_V(!is_library_open(), ERR_UNCONFIGURED);
	ERR_FAIL_COND_V(p_level < INITIALIZATION_LEVEL_CORE || p_level > INITIALIZATION_LEVEL_EDITOR, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(p_level > int32_t(level_initialized), ERR_INVALID_PARAMETER);
	ERR_FAIL_NULL_V(initialization.deinitialize, ERR_UNCONFIGURED);

	GDExtensionDeviceExit::LevelScope callback_scope(library_exit_ref, this, initialization, int32_t(p_level), false);
	ERR_FAIL_COND_V(callback_scope.get_error() != OK, callback_scope.get_error());
	level_initialized = int32_t(p_level) - 1;
	initialization.deinitialize(initialization.userdata, GDExtensionInitializationLevel(p_level));
	return OK; // Dispatch completion only; not a successful finalization witness.
}

Error GDExtension::abort_scene_ready_load(Error original_error) {
	if (original_error <= OK || original_error >= ERR_MAX) { return ERR_INVALID_PARAMETER; }
	using namespace GDExtensionDeviceExit;
	require_close_success(preflight_exit(CloseRoute::INIT_ABORT), CloseRoute::INIT_ABORT);
#ifdef TOOLS_ENABLED
	// Keep cached MethodBinds as invalid native metadata, following the existing
	// reload strategy; never let ordinary unregister delete a reused cache.
	scene_ready_aborting = true;
#endif
	const int32_t minimum = initialization.minimum_initialization_level;
	while (level_initialized >= minimum) {
		const int32_t actual_level = level_initialized;
		require_level_dispatch_success(deinitialize_library(InitializationLevel(actual_level)), actual_level, false);
	}
#ifdef TOOLS_ENABLED
	clear_instance_bindings(); // Real free callbacks, before the native close.
	for (KeyValue<StringName, Extension> &entry : extension_classes) {
		// Every new class must have returned its real binding deinitializer.
		// Old unregistered reload shells have no gdtype and must not be cleared
		// again, reconstructed, notified, or passed to a closed generation.
		require_close_success(entry.value.gdextension.gdtype ? ERR_BUSY : OK, CloseRoute::INIT_ABORT);
		for (KeyValue<StringName, GDExtensionMethodBind *> &method : entry.value.methods) {
			method.value->valid = false;
			method.value->is_reloading = false;
			if (invalid_methods.find(method.value) == -1) { invalid_methods.push_back(method.value); }
		}
		entry.value.methods.clear();
		entry.value.instance_state.clear(); // Variant destruction stays before close and under original guards.
		entry.value.instances.clear();
	}
	extension_classes.clear();
	is_reloading = false;
	scene_ready_aborting = false;
	has_get_classes_used_callback = false;
	get_classes_used_callback = nullptr;
#endif
	startup_callback = shutdown_callback = frame_callback = nullptr;
	main_loop_callback_mask = 0;
	require_close_success(_close_library(CloseRoute::INIT_ABORT), CloseRoute::INIT_ABORT);
	return original_error;
}

void GDExtension::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_library_open"), &GDExtension::is_library_open);
	ClassDB::bind_method(D_METHOD("get_minimum_library_initialization_level"), &GDExtension::get_minimum_library_initialization_level);

	BIND_ENUM_CONSTANT(INITIALIZATION_LEVEL_CORE);
	BIND_ENUM_CONSTANT(INITIALIZATION_LEVEL_SERVERS);
	BIND_ENUM_CONSTANT(INITIALIZATION_LEVEL_SCENE);
	BIND_ENUM_CONSTANT(INITIALIZATION_LEVEL_EDITOR);
}

GDExtension::~GDExtension() {
	if (library_close_error != OK || is_library_open()) {
		const auto route = library_exit_route_selected ? library_exit_route : GDExtensionDeviceExit::CloseRoute::LAST_REF;
		GDExtensionDeviceExit::require_close_success(_close_library(route), route);
	}
	GDExtensionDeviceExit::Registry::get_singleton().detach(library_exit_ref);
	library_exit_ref.reset();
	GDExtensionDeviceExit::Registry::get_singleton().forget_binding_owner(this);
#ifdef TOOLS_ENABLED
	// If we have any invalid method binds still laying around, we can finally free them!
	for (GDExtensionMethodBind *E : invalid_methods) {
		memdelete(E);
	}
#endif
}

Error GDExtension::initialize_gdextensions(uint64_t &r_cycle) {
	const Error err = GDExtensionDeviceExit::Registry::get_singleton().begin_cycle(r_cycle);
	if (err != OK) {
		return err;
	}
	gdextension_setup_interface();
	GDExtensionDeviceExit::Registry::register_interfaces();

#ifndef DISABLE_DEPRECATED
	register_interface_function("classdb_register_extension_class", (GDExtensionInterfaceFunctionPtr)&GDExtension::_register_extension_class);
	register_interface_function("classdb_register_extension_class2", (GDExtensionInterfaceFunctionPtr)&GDExtension::_register_extension_class2);
	register_interface_function("classdb_register_extension_class3", (GDExtensionInterfaceFunctionPtr)&GDExtension::_register_extension_class3);
	register_interface_function("classdb_register_extension_class4", (GDExtensionInterfaceFunctionPtr)&GDExtension::_register_extension_class4);
	register_interface_function("classdb_register_extension_class5", (GDExtensionInterfaceFunctionPtr)&GDExtension::_register_extension_class5);
#endif // DISABLE_DEPRECATED
	register_interface_function("classdb_register_extension_class6", (GDExtensionInterfaceFunctionPtr)&GDExtension::_register_extension_class6);
	register_interface_function("classdb_register_extension_class_method", (GDExtensionInterfaceFunctionPtr)&GDExtension::_register_extension_class_method);
	register_interface_function("classdb_register_extension_class_virtual_method", (GDExtensionInterfaceFunctionPtr)&GDExtension::_register_extension_class_virtual_method);
	register_interface_function("classdb_register_extension_class_integer_constant", (GDExtensionInterfaceFunctionPtr)&GDExtension::_register_extension_class_integer_constant);
	register_interface_function("classdb_register_extension_class_property", (GDExtensionInterfaceFunctionPtr)&GDExtension::_register_extension_class_property);
	register_interface_function("classdb_register_extension_class_property_indexed", (GDExtensionInterfaceFunctionPtr)&GDExtension::_register_extension_class_property_indexed);
	register_interface_function("classdb_register_extension_class_property_group", (GDExtensionInterfaceFunctionPtr)&GDExtension::_register_extension_class_property_group);
	register_interface_function("classdb_register_extension_class_property_subgroup", (GDExtensionInterfaceFunctionPtr)&GDExtension::_register_extension_class_property_subgroup);
	register_interface_function("classdb_register_extension_class_signal", (GDExtensionInterfaceFunctionPtr)&GDExtension::_register_extension_class_signal);
	register_interface_function("classdb_unregister_extension_class", (GDExtensionInterfaceFunctionPtr)&GDExtension::_unregister_extension_class);
	register_interface_function("get_library_path", (GDExtensionInterfaceFunctionPtr)&GDExtension::_get_library_path);
	register_interface_function("editor_register_get_classes_used_callback", (GDExtensionInterfaceFunctionPtr)&GDExtension::_register_get_classes_used_callback);
	register_interface_function("register_main_loop_callbacks", (GDExtensionInterfaceFunctionPtr)&GDExtension::_register_main_loop_callbacks);
	return OK;
}

void GDExtension::finalize_gdextensions(uint64_t p_cycle) {
	if (GDExtensionDeviceExit::Registry::get_singleton().begin_finalizing(p_cycle)) {
		gdextension_interface_functions.clear();
	}
}

#ifdef TOOLS_ENABLED

bool GDExtension::has_library_changed() const {
	return loader->has_library_changed();
}

void GDExtension::prepare_reload() {
	is_reloading = true;

	for (KeyValue<StringName, Extension> &E : extension_classes) {
		E.value.is_reloading = true;

		for (KeyValue<StringName, GDExtensionMethodBind *> &M : E.value.methods) {
			M.value->is_reloading = true;
		}

		for (const ObjectID &obj_id : E.value.instances) {
			Object *obj = ObjectDB::get_instance(obj_id);
			if (!obj) {
				continue;
			}

			// Store instance state so it can be restored after reload.
			List<Pair<String, Variant>> state;
			List<PropertyInfo> prop_list;
			obj->get_property_list(&prop_list);
			for (const PropertyInfo &P : prop_list) {
				if (!(P.usage & PROPERTY_USAGE_STORAGE)) {
					continue;
				}

				Variant value = obj->get(P.name);
				Variant default_value = ClassDB::class_get_default_property_value(obj->get_class_name(), P.name);

				if (default_value.get_type() != Variant::NIL && bool(Variant::evaluate(Variant::OP_EQUAL, value, default_value))) {
					continue;
				}

				if (P.type == Variant::OBJECT && value.is_zero() && !(P.usage & PROPERTY_USAGE_STORE_IF_NULL)) {
					continue;
				}

				state.push_back(Pair<String, Variant>(P.name, value));
			}
			E.value.instance_state[obj_id] = {
				std::move(state), // List<Pair<String, Variant>> properties;
				obj->is_extension_placeholder(), // bool is_placeholder;
			};
		}
	}
}

void GDExtension::_clear_extension(Extension *p_extension) {
	// Clear out hierarchy information because it may change.
	p_extension->gdextension.parent = nullptr;
	p_extension->gdextension.children.clear();

	// Clear all objects of any GDExtension data. It will become its native parent class
	// until the reload can reset the object with the new GDExtension data.
	Vector<ObjectID> instances;
	for (const ObjectID &obj_id : p_extension->instances) { instances.push_back(obj_id); }
	for (const ObjectID &obj_id : instances) {
		Object *obj = ObjectDB::get_instance(obj_id);
		if (!obj) {
			continue;
		}

		if (obj->_get_extension() == &p_extension->gdextension) { obj->clear_internal_extension(); }
	}

	if (p_extension->gdextension.gdtype) { p_extension->gdextension.destroy_gdtype(); }
}

void GDExtension::track_instance_binding(Object *p_object) {
	instance_bindings.push_back(p_object->get_instance_id());
}

void GDExtension::untrack_instance_binding(Object *p_object) {
	instance_bindings.erase(p_object->get_instance_id());
}

void GDExtension::clear_instance_bindings() {
	// Free callbacks may untrack themselves or destroy another tracked object.
	// Keep the original primitive-ID snapshot alive across those real returns.
	const Vector<ObjectID> bindings = instance_bindings;
	for (ObjectID obj_id : bindings) {
		Object *obj = ObjectDB::get_instance(obj_id);
		if (!obj) {
			continue;
		}

		obj->free_instance_binding(this);
	}
	instance_bindings.clear();
}

void GDExtension::finish_reload() {
	is_reloading = false;

	// Clean up any classes or methods that didn't get re-added.
	Vector<StringName> classes_to_remove;
	for (KeyValue<StringName, Extension> &E : extension_classes) {
		if (E.value.is_reloading) {
			E.value.is_reloading = false;
			classes_to_remove.push_back(E.key);
		}

		Vector<StringName> methods_to_remove;
		for (KeyValue<StringName, GDExtensionMethodBind *> &M : E.value.methods) {
			if (M.value->is_reloading) {
				M.value->valid = false;
				invalid_methods.push_back(M.value);

				M.value->is_reloading = false;
				methods_to_remove.push_back(M.key);
			}
		}
		for (const StringName &method_name : methods_to_remove) {
			E.value.methods.erase(method_name);
		}
	}
	for (const StringName &class_name : classes_to_remove) {
		extension_classes.erase(class_name);
	}

	// Reset any the extension on instances made from the classes that remain.
	for (KeyValue<StringName, Extension> &E : extension_classes) {
		// Loop over 'instance_state' rather than 'instance' because new instances
		// may have been created when re-initializing the extension.
		for (const KeyValue<ObjectID, Extension::InstanceState> &S : E.value.instance_state) {
			Object *obj = ObjectDB::get_instance(S.key);
			if (!obj) {
				continue;
			}

			if (S.value.is_placeholder) {
				obj->reset_internal_extension(ClassDB::get_placeholder_extension(E.value.gdextension.class_name));
			} else {
				obj->reset_internal_extension(&E.value.gdextension);
			}
		}
	}

	// Now that all the classes are back, restore the state.
	for (KeyValue<StringName, Extension> &E : extension_classes) {
		for (const KeyValue<ObjectID, Extension::InstanceState> &S : E.value.instance_state) {
			Object *obj = ObjectDB::get_instance(S.key);
			if (!obj) {
				continue;
			}

			for (const Pair<String, Variant> &state : S.value.properties) {
				obj->set(state.first, state.second);
			}
		}
	}

	// Finally, let the objects know that we are done reloading them.
	for (KeyValue<StringName, Extension> &E : extension_classes) {
		for (const KeyValue<ObjectID, Extension::InstanceState> &S : E.value.instance_state) {
			Object *obj = ObjectDB::get_instance(S.key);
			if (!obj) {
				continue;
			}

			obj->notification(NOTIFICATION_EXTENSION_RELOADED);
		}

		// Clear the instance state, we're done looping.
		E.value.instance_state.clear();
	}
}

void GDExtension::_track_instance(void *p_user_data, void *p_instance) {
	Extension *extension = reinterpret_cast<Extension *>(p_user_data);
	Object *obj = reinterpret_cast<Object *>(p_instance);

	extension->instances.insert(obj->get_instance_id());
}

void GDExtension::_untrack_instance(void *p_user_data, void *p_instance) {
	Extension *extension = reinterpret_cast<Extension *>(p_user_data);
	Object *obj = reinterpret_cast<Object *>(p_instance);

	extension->instances.erase(obj->get_instance_id());
}

PackedStringArray GDExtension::get_classes_used() const {
	PackedStringArray ret;
	if (!has_get_classes_used_callback) {
		return ret;
	}
	GDExtensionDeviceExit::CallbackScope callback_scope(library_exit_ref);
	ERR_FAIL_COND_V(callback_scope.get_error() != OK, ret);
	if (get_classes_used_callback) {
		get_classes_used_callback((GDExtensionTypePtr)&ret);
	}
	return ret;
}

void GDExtensionEditorPlugins::add_extension_class(const StringName &p_class_name) {
	if (editor_node_add_plugin) {
		callable_mp_static(editor_node_add_plugin).call_deferred(p_class_name);
	} else {
		extension_classes.push_back(p_class_name);
	}
}

void GDExtensionEditorPlugins::remove_extension_class(const StringName &p_class_name) {
	if (editor_node_remove_plugin) {
		editor_node_remove_plugin(p_class_name);
	} else {
		extension_classes.erase(p_class_name);
	}
}

void GDExtensionEditorHelp::load_xml_buffer(const uint8_t *p_buffer, int p_size) {
	ERR_FAIL_NULL(editor_help_load_xml_buffer);
	editor_help_load_xml_buffer(p_buffer, p_size);
}

void GDExtensionEditorHelp::remove_class(const String &p_class) {
	ERR_FAIL_NULL(editor_help_remove_class);
	editor_help_remove_class(p_class);
}
#endif // TOOLS_ENABLED
