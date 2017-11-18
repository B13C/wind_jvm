/*
 * oop.cpp
 *
 *  Created on: 2017年11月12日
 *      Author: zhengxiaolin
 */

#include "runtime/oop.hpp"
#include "classloader.hpp"

/*===----------------  InstanceOop  -----------------===*/
InstanceOop::InstanceOop(shared_ptr<InstanceKlass> klass) : Oop(klass, OopType::_InstanceOop) {
	// alloc non-static-field memory.
	this->field_length = klass->non_static_field_num();
	std::wcout << klass->get_name() << "'s field_size allocate " << this->field_length << " bytes..." << std::endl;	// delete
	if (this->field_length != 0) {
		fields = new Oop*[this->field_length];			// TODO: not gc control......
		memset(fields, 0, this->field_length * sizeof(Oop *));		// 啊啊啊啊全部清空别忘了！！
	}

	// initialize BasicTypeOop...
	klass->initialize_field(klass->fields_layout, this->fields);
}

bool InstanceOop::get_field_value(shared_ptr<Field_info> field, Oop **result)
{
	shared_ptr<InstanceKlass> instance_klass = std::static_pointer_cast<InstanceKlass>(this->klass);
	wstring signature = field->get_name() + L":" + field->get_descriptor();
	return get_field_value(signature, result);
}

void InstanceOop::set_field_value(shared_ptr<Field_info> field, Oop *value)
{
	shared_ptr<InstanceKlass> instance_klass = std::static_pointer_cast<InstanceKlass>(this->klass);
	wstring signature = field->get_name() + L":" + field->get_descriptor();
	set_field_value(signature, value);
}

bool InstanceOop::get_field_value(const wstring & signature, Oop **result) 				// use for forging String Oop at parsing constant_pool.
{		// [bug发现] 在 ByteCodeEngine 中，getField 里边，由于我的设计，因此即便是读取 int 也会返回一个 IntOop 的 对象。因此这肯定是错误的... 改为在这里直接取引用，直接解除类型并且取出真值。
	shared_ptr<InstanceKlass> instance_klass = std::static_pointer_cast<InstanceKlass>(this->klass);
	auto iter = instance_klass->fields_layout.find(signature);		// non-static field 由于复制了父类中的所有 field (继承)，所以只在 this_klass 中查找！
	if (iter == instance_klass->fields_layout.end()) {
		std::wcerr << "didn't find field [" << signature << "] in InstanceKlass " << instance_klass->name << std::endl;
		assert(false);
	}
	int offset = iter->second.first;
	// field value not 0, maybe basic type.
	*result = this->fields[offset];
	return true;
}

void InstanceOop::set_field_value(const wstring & signature, Oop *value)
{
	shared_ptr<InstanceKlass> instance_klass = std::static_pointer_cast<InstanceKlass>(this->klass);
	auto iter = instance_klass->fields_layout.find(signature);
	if (iter == instance_klass->fields_layout.end()) {
		std::wcerr << "didn't find field [" << signature << "] in InstanceKlass " << instance_klass->name << std::endl;
		assert(false);
	}
	int offset = iter->second.first;
	// field value not 0, maybe basic type.
	this->fields[offset] = value;
}

/*===----------------  MirrorOop  -------------------===*/
MirrorOop::MirrorOop(shared_ptr<Klass> mirrored_who) : mirrored_who(mirrored_who),
					InstanceOop(std::static_pointer_cast<InstanceKlass>(BootStrapClassLoader::get_bootstrap().loadClass(L"java/lang/Class"))) {}

/*===----------------  TypeArrayOop  -------------------===*/

/*===----------------  BasicTypeOop  -------------------===*/
uint64_t BasicTypeOop::get_value()
{
	switch (type) {
		case Type::BYTE:
			return ((ByteOop *)this)->value;
		case Type::BOOLEAN:
			return ((BooleanOop *)this)->value;
		case Type::CHAR:
			return ((CharOop *)this)->value;
		case Type::SHORT:
			return ((ShortOop *)this)->value;
		case Type::INT:
			return ((IntOop *)this)->value;
		case Type::FLOAT:
			return ((FloatOop *)this)->value;
		case Type::LONG:
			return ((LongOop *)this)->value;
		case Type::DOUBLE:
			return ((DoubleOop *)this)->value;
		default:{
			std::cerr << "can't get here!" << std::endl;
			assert(false);
		}
	}
}

void BasicTypeOop::set_value(uint64_t value)
{
	switch (type) {
		case Type::BYTE:
			((ByteOop *)this)->value = value;
			break;
		case Type::BOOLEAN:
			((BooleanOop *)this)->value = value;
			break;
		case Type::CHAR:
			((CharOop *)this)->value = value;
			break;
		case Type::SHORT:
			((ShortOop *)this)->value = value;
			break;
		case Type::INT:
			((IntOop *)this)->value = value;
			break;
		case Type::FLOAT:
			((FloatOop *)this)->value = value;
			break;
		case Type::LONG:
			((LongOop *)this)->value = value;
			break;
		case Type::DOUBLE:
			((DoubleOop *)this)->value = value;
			break;
		default:{
			std::cerr << "can't get here!" << std::endl;
			assert(false);
		}
	}
}
