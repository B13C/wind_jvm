/*
 * java_lang_invoke_MethodHandleNatives.cpp
 *
 *  Created on: 2017年12月9日
 *      Author: zhengxiaolin
 */

#include "native/java_lang_invoke_MethodHandleNatives.hpp"
#include <vector>
#include <algorithm>
#include <cassert>
#include "native/native.hpp"
#include "native/java_lang_String.hpp"
#include "utils/os.hpp"
#include "classloader.hpp"
#include "wind_jvm.hpp"

static unordered_map<wstring, void*> methods = {
    {L"getConstant:(I)I",							(void *)&JVM_GetConstant},
    {L"resolve:(" MN CLS ")" MN,						(void *)&JVM_Resolve},
    {L"expand:(" MN ")V",							(void *)&JVM_Expand},
    {L"init:(" MN OBJ ")V",							(void *)&JVM_Init},
};

void JVM_GetConstant(list<Oop *> & _stack){		// static
	int num = ((IntOop *)_stack.front())->value;	_stack.pop_front();
	assert(num == 4);		// 看源码得到的...
	_stack.push_back(new IntOop(false));		// 我就 XJB 返回了......看到源码好像没有用到这的...而且也看不太懂....定义 COMPILER2 宏就 true ???
}

wstring get_full_name(MirrorOop *mirror)
{
	auto klass = mirror->get_mirrored_who();
	if (klass == nullptr) {		// primitive
		return mirror->get_extra();
	} else {
		if (klass->get_type() == ClassType::InstanceClass) {
			return L"L" + klass->get_name() + L";";
		} else if (klass->get_type() == ClassType::ObjArrayClass || klass->get_type() == ClassType::TypeArrayClass){
			return klass->get_name();
		} else {
			assert(false);
		}
	}
}

void JVM_Resolve(list<Oop *> & _stack){		// static
	InstanceOop *member_name_obj = (InstanceOop *)_stack.front();	_stack.pop_front();
	MirrorOop *caller_mirror = (MirrorOop *)_stack.front();	_stack.pop_front();		// I ignored it.

	if (member_name_obj == nullptr) {
		assert(false);		// TODO: throw InternalError
	}

	Oop *oop;
	member_name_obj->get_field_value(MEMBERNAME L":clazz:" CLS, &oop);
	MirrorOop *clazz = (MirrorOop *)oop;		// e.g.: Test8
	assert(clazz != nullptr);
	member_name_obj->get_field_value(MEMBERNAME L":name:" STR, &oop);
	InstanceOop *name = (InstanceOop *)oop;	// e.g.: doubleVal
	assert(name != nullptr);
	member_name_obj->get_field_value(MEMBERNAME L":type:" OBJ, &oop);
	InstanceOop *type = (InstanceOop *)oop;	// maybe a String, or maybe an Object[]...
			// type 这个变量的类型可能是 String, Class, MethodType!
	assert(type != nullptr);
	member_name_obj->get_field_value(MEMBERNAME L":flags:I", &oop);
	int flags = ((IntOop *)oop)->value;

//	std::wcout << clazz->get_mirrored_who()->get_name() << " " << java_lang_string::stringOop_to_wstring(name) << std::endl;		// delete

	auto klass = clazz->get_mirrored_who();

	// decode is from openjdk:
	int ref_kind = ((flags & 0xF000000) >> 24);
	/**
	 * from 1 ~ 9:
	 * 1: getField
	 * 2: getStatic
	 * 3: putField
	 * 4: putStatic
	 * 5: invokeVirtual
	 * 6: invokeStatic
	 * 7: invokeSpecial
	 * 8: newInvokeSpecial
	 * 9: invokeInterface
	 */
	assert(ref_kind >= 1 && ref_kind <= 9);

	if (klass == nullptr) {
		assert(false);
	} else if (klass->get_type() == ClassType::InstanceClass) {
		// right. do nothing.
	} else if (klass->get_type() == ClassType::ObjArrayClass || klass->get_type() == ClassType::TypeArrayClass) {
		klass = BootStrapClassLoader::get_bootstrap().loadClass(L"java/lang/Object");		// TODO: 并不清楚为何要这样替换。
	} else {
		assert(false);
	}

	auto real_klass = std::static_pointer_cast<InstanceKlass>(klass);
	wstring real_name = java_lang_string::stringOop_to_wstring(name);
	if (real_name == L"<clinit>" || real_name == L"<init>") {
		assert(false);		// can't be the two names.
	}

	// 0. create a empty wstring: descriptor
	wstring descriptor;
	// 0.5. if we should 钦定 these blow: only for real_klass is `java/lang/invoke/MethodHandle`:
	if (real_klass->get_name() == L"java/lang/invoke/MethodHandle" &&
				(real_name == L"invoke"
				|| real_name == L"invokeBasic"				// 钦定这些。因为 else 中都是通过把 ptypes 加起来做到的。但是 MethodHandle 中的变长参数是一个例外。其他类中的变长参数没问题，因为最后编译器会全部转为 Object[]。只有 MethodHandle 是例外～
				|| real_name == L"invokeExact"
				|| real_name == L"invokeWithArauments"
				|| real_name == L"linkToSpecial"
				|| real_name == L"linkToStatic"
				|| real_name == L"linkToVirtual"
				|| real_name == L"linkToInterface"))  {		// 悲伤。由于历史原因（，我的查找是通过字符串比对来做的......简直无脑啊......这样这里效率好低吧QAQ。不过毕竟只是个玩具，跑通就好......
		descriptor = L"([Ljava/lang/Object;)Ljava/lang/Object;";
	} else {
		// 1. should parse the `Object type;` member first.
		if (type->get_klass()->get_name() == L"java/lang/invoke/MethodType") {
			descriptor += L"(";
			Oop *oop;
			// 1-a-1: get the args type.
			type->get_field_value(METHODTYPE L":ptypes:[" CLS, &oop);
			assert(oop != nullptr);
			auto class_arr_obj = (ArrayOop *)oop;
			for (int i = 0; i < class_arr_obj->get_length(); i ++) {
				descriptor += get_full_name((MirrorOop *)(*class_arr_obj)[i]);
			}
			descriptor += L")";
			// 1-a-2: get the return type.
			type->get_field_value(METHODTYPE L":rtype:" CLS, &oop);
			assert(oop != nullptr);
			descriptor += get_full_name((MirrorOop *)oop);
		} else if (type->get_klass()->get_name() == L"java/lang/Class") {
			auto real_klass = ((MirrorOop *)type)->get_mirrored_who();
			if (real_klass == nullptr) {
				descriptor += ((MirrorOop *)type)->get_extra();
			} else {
				if (real_klass->get_type() == ClassType::InstanceClass) {
					descriptor += (L"L" + real_klass->get_name() + L";");
				} else if (real_klass->get_type() == ClassType::TypeArrayClass || real_klass->get_type() == ClassType::ObjArrayClass) {
					descriptor += real_klass->get_name();
				} else {
					assert(false);
				}
			}
		} else if (type->get_klass()->get_name() == L"java/lang/String") {
			assert(false);		// not support yet...
		} else {
			assert(false);
		}
	}


	if (flags & 0x10000) {		// Method:
		wstring signature = real_name + L":" + descriptor;
		shared_ptr<Method> target_method;
		if (ref_kind == 6)	{	// invokeStatic
			std::wcout << real_klass->get_name() << " " << signature << std::endl;	// delete
			target_method = real_klass->get_this_class_method(signature);
			assert(target_method != nullptr);
		} else {
			std::wcout << ".....signature: [" << real_klass->get_name() << " " << real_name << " " << descriptor << std::endl;	// delete
			vm_thread *thread = (vm_thread *)_stack.back();		// delete
			thread->get_stack_trace();			// delete
			assert(false);		// not support yet...
		}

		// build the return MemberName obj.
		auto member_name2 = std::static_pointer_cast<InstanceKlass>(member_name_obj->get_klass())->new_instance();
		int new_flag = (target_method->get_flag() & (~ACC_ANNOTATION));
		if (target_method->has_annotation_name_in_method(L"Lsun/reflect/CallerSensitive;")) {
			new_flag |= 0x100000;
		}
		if (ref_kind == 6) {
			new_flag |= 0x10000 | (6 << 24);		// invokeStatic
		} else {
			assert(false);		// not support yet...
		}

		member_name2->set_field_value(MEMBERNAME L":flags:I", new IntOop(new_flag));
		member_name2->set_field_value(MEMBERNAME L":name:" STR, name);
		member_name2->set_field_value(MEMBERNAME L":type:" OBJ, type);
		member_name2->set_field_value(MEMBERNAME L":clazz:" CLS, target_method->get_klass()->get_mirror());
		_stack.push_back(member_name2);
		return;
	} else if (flags & 0x20000){		// Constructor
		assert(false);			// not support yet...
	} else if (flags & 0x40000) {	// Field
		wstring signature = real_name + L":" + descriptor;
		auto _pair = real_klass->get_field(signature);
		assert(_pair.second != nullptr);
		auto target_field = _pair.second;

		// very **IMPORTANT**!! used for the `target_field->get_type_klass()` after!
		target_field->if_didnt_parse_then_parse();

		// build the return MemberName obj.
		auto member_name2 = std::static_pointer_cast<InstanceKlass>(member_name_obj->get_klass())->new_instance();
		int new_flag = (target_field->get_flag() & (~ACC_ANNOTATION));
		if (target_field->is_static()) {
			new_flag |= 0x40000 | (2 << 24);		// getStatic(2)
		} else {
			new_flag |= 0x40000 | (1 << 24);		// getField(1)
		}

		if (ref_kind > 2) {		// putField(3) / putStatic(2)
			new_flag += ((3 - 1) << 24);
		}

		member_name2->set_field_value(MEMBERNAME L":flags:I", new IntOop(new_flag));
		member_name2->set_field_value(MEMBERNAME L":name:" STR, name);
		member_name2->set_field_value(MEMBERNAME L":type:" OBJ, java_lang_string::intern(target_field->get_descriptor()));
		member_name2->set_field_value(MEMBERNAME L":clazz:" CLS, target_field->get_type_klass()->get_mirror());
		_stack.push_back(member_name2);
		return;
	} else {
		assert(false);
	}



	assert(false);
}

void JVM_Expand(list<Oop *> & _stack) {
	// 这个方法无法使用。因为我的实现没有 itable...而且我也并不知道 vmindex 应该被放到哪里。
}

void JVM_Init(list<Oop *> & _stack){		// static
	InstanceOop *member_name_obj = (InstanceOop *)_stack.front();	_stack.pop_front();
	InstanceOop *target = (InstanceOop *)_stack.front();	_stack.pop_front();			// java/lang/Object

	/**
	 * in fact, `target` will be one of the three:
	 * 1. java/lang/reflect/Constructor.
	 * 2. java/lang/reflect/Field.
	 * 3. java/lang/reflect/Method.
	 */
	assert(member_name_obj != nullptr);
	assert(target != nullptr);
	auto klass = std::static_pointer_cast<InstanceKlass>(target->get_klass());

	if (klass->get_name() == L"java/lang/reflect/Constructor") {
		Oop *oop;
		target->get_field_value(CONSTRUCTOR L":slot:I", &oop);
		int slot = ((IntOop *)oop)->value;
		shared_ptr<Method> target_method = klass->search_method_in_slot(slot);

		int new_flag = (target_method->get_flag() & (~ACC_ANNOTATION));
		if (target_method->has_annotation_name_in_method(L"Lsun/reflect/CallerSensitive;")) {
			new_flag |= 0x100000;
		}

		new_flag |= 0x10000 | (7 << 24);		// invokeSpecial: 7

		member_name_obj->set_field_value(MEMBERNAME L":flags:I", new IntOop(new_flag));
//		member_name_obj->set_field_value(MEMBERNAME L":name:" STR, name);		// in JDK source code: MemberName::public MemberName(Field fld, boolean makeSetter), the `name` and `type` are settled by the java source code.
//		member_name_obj->set_field_value(MEMBERNAME L":type:" OBJ, type);
		member_name_obj->set_field_value(MEMBERNAME L":clazz:" CLS, klass->get_mirror());

	} else if (klass->get_name() == L"java/lang/reflect/Field") {
		Oop *oop;
		target->get_field_value(FIELD L":modifiers:I", &oop);
		int new_flag = ((IntOop *)oop)->value;
		target->get_field_value(FIELD L":clazz:" CLS, &oop);
		Oop *clazz = oop;

		new_flag = (new_flag & (~ACC_ANNOTATION));

		member_name_obj->set_field_value(MEMBERNAME L":flags:I", new IntOop(new_flag));
//		member_name_obj->set_field_value(MEMBERNAME L":name:" STR, name);
//		member_name_obj->set_field_value(MEMBERNAME L":type:" OBJ, type);
		member_name_obj->set_field_value(MEMBERNAME L":clazz:" CLS, clazz);

	} else if (klass->get_name() == L"java/lang/reflect/Method") {
		Oop *oop;
		target->get_field_value(METHOD L":slot:I", &oop);
		int slot = ((IntOop *)oop)->value;
		shared_ptr<Method> target_method = klass->search_method_in_slot(slot);

		int new_flag = (target_method->get_flag() & (~ACC_ANNOTATION));
		if (target_method->has_annotation_name_in_method(L"Lsun/reflect/CallerSensitive;")) {
			new_flag |= 0x100000;
		}

		if (target_method->is_private() && !target_method->is_static()) {		// use invokeSpecial.
			new_flag |= 0x10000 | (7 << 24);		// invokeSpecial: 7
		} else if (target_method->is_static()) {
			new_flag |= 0x10000 | (6 << 24);		// invokeStatic: 6
		} else {
			if (klass->is_in_vtable(target_method)) {
				new_flag |= 0x10000 | (5 << 24);		// invokeVirtual: 5
			} else {
				new_flag |= 0x10000 | (9 << 24);		// invokeInterface: 9
			}
		}

		member_name_obj->set_field_value(MEMBERNAME L":flags:I", new IntOop(new_flag));
//		member_name_obj->set_field_value(MEMBERNAME L":name:" STR, name);		// in JDK source code: MemberName::public MemberName(Field fld, boolean makeSetter), the `name` and `type` are settled by the java source code.
//		member_name_obj->set_field_value(MEMBERNAME L":type:" OBJ, type);
		member_name_obj->set_field_value(MEMBERNAME L":clazz:" CLS, klass->get_mirror());

	} else {
		assert(false);
	}


}


// 返回 fnPtr.
void *java_lang_invoke_methodHandleNatives_search_method(const wstring & signature)
{
	auto iter = methods.find(signature);
	if (iter != methods.end()) {
		return (*iter).second;
	}
	return nullptr;
}

