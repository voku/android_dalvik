/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "Dalvik.h"
#include "alloc/clz.h"
#include "alloc/Visit.h"

/*
 * Visits the instance fields of a class or data object.
 */
static void visitInstanceFields(Visitor *visitor, Object *obj)
{
    assert(visitor != NULL);
    assert(obj != NULL);
    assert(obj->clazz != NULL);
    LOGV("Entering visitInstanceFields(visitor=%p,obj=%p)", visitor, obj);
    if (obj->clazz->refOffsets != CLASS_WALK_SUPER) {
        size_t refOffsets = obj->clazz->refOffsets;
        while (refOffsets != 0) {
            size_t rshift = CLZ(refOffsets);
            size_t offset = CLASS_OFFSET_FROM_CLZ(rshift);
            Object **ref = BYTE_OFFSET(obj, offset);
            (*visitor)(ref);
            refOffsets &= ~(CLASS_HIGH_BIT >> rshift);
        }
    } else {
        ClassObject *clazz;
        for (clazz = obj->clazz; clazz != NULL; clazz = clazz->super) {
            InstField *field = clazz->ifields;
            int i;
            for (i = 0; i < clazz->ifieldRefCount; ++i, ++field) {
                size_t offset = field->byteOffset;
                Object **ref = BYTE_OFFSET(obj, offset);
                (*visitor)(ref);
            }
        }
    }
    LOGV("Exiting visitInstanceFields(visitor=%p,obj=%p)", visitor, obj);
}

/*
 * Visits the static fields of a class object.
 */
static void visitStaticFields(Visitor *visitor, ClassObject *clazz)
{
    int i;

    assert(visitor != NULL);
    assert(clazz != NULL);
    for (i = 0; i < clazz->sfieldCount; ++i) {
        char ch = clazz->sfields[i].field.signature[0];
        if (ch == '[' || ch == 'L') {
            (*visitor)(&clazz->sfields[i].value.l);
        }
    }
}

/*
 * Visit the interfaces of a class object.
 */
static void visitInterfaces(Visitor *visitor, ClassObject *clazz)
{
    int i;

    assert(visitor != NULL);
    assert(clazz != NULL);
    for (i = 0; i < clazz->interfaceCount; ++i) {
        (*visitor)(&clazz->interfaces[i]);
    }
}

/*
 * Visits all the references stored in a class object instance.
 */
static void visitClassObject(Visitor *visitor, ClassObject *obj)
{
    assert(visitor != NULL);
    assert(obj != NULL);
    LOGV("Entering visitClassObject(visitor=%p,obj=%p)", visitor, obj);
    assert(!strcmp(obj->obj.clazz->descriptor, "Ljava/lang/Class;"));
    (*visitor)(&obj->obj.clazz);
    if (IS_CLASS_FLAG_SET(obj, CLASS_ISARRAY)) {
        (*visitor)(&obj->elementClass);
    }
    if (obj->status > CLASS_IDX) {
        (*visitor)(&obj->super);
    }
    (*visitor)(&obj->classLoader);
    visitInstanceFields(visitor, (Object *)obj);
    visitStaticFields(visitor, obj);
    if (obj->status > CLASS_IDX) {
        visitInterfaces(visitor, obj);
    }
    LOGV("Exiting visitClassObject(visitor=%p,obj=%p)", visitor, obj);
}

/*
 * Visits the class object and, if the array is typed as an object
 * array, all of the array elements.
 */
static void visitArrayObject(Visitor *visitor, Object *obj)
{
    assert(visitor != NULL);
    assert(obj != NULL);
    assert(obj->clazz != NULL);
    LOGV("Entering visitArrayObject(visitor=%p,obj=%p)", visitor, obj);
    (*visitor)(&obj->clazz);
    if (IS_CLASS_FLAG_SET(obj->clazz, CLASS_ISOBJECTARRAY)) {
        ArrayObject *array = (ArrayObject *)obj;
        Object **contents = (Object **)array->contents;
        size_t i;
        for (i = 0; i < array->length; ++i) {
            (*visitor)(&contents[i]);
        }
    }
    LOGV("Exiting visitArrayObject(visitor=%p,obj=%p)", visitor, obj);
}

/*
 * Visits the class object and reference typed instance fields of a
 * data object.
 */
static void visitDataObject(Visitor *visitor, Object *obj)
{
    assert(visitor != NULL);
    assert(obj != NULL);
    assert(obj->clazz != NULL);
    LOGV("Entering visitDataObject(visitor=%p,obj=%p)", visitor, obj);
    (*visitor)(&obj->clazz);
    visitInstanceFields(visitor, obj);
    if (IS_CLASS_FLAG_SET(obj->clazz, CLASS_ISREFERENCE)) {
        size_t offset = gDvm.offJavaLangRefReference_referent;
        Object **ref = BYTE_OFFSET(obj, offset);
        (*visitor)(ref);
    }
    LOGV("Exiting visitDataObject(visitor=%p,obj=%p)", visitor, obj);
}

/*
 * Visits all of the reference stored in an object.
 */
void dvmVisitObject(Visitor *visitor, Object *obj)
{
    assert(visitor != NULL);
    assert(obj != NULL);
    assert(obj->clazz != NULL);
    LOGV("Entering dvmVisitObject(visitor=%p,obj=%p)", visitor, obj);
    if (obj->clazz == gDvm.classJavaLangClass) {
        visitClassObject(visitor, (ClassObject *)obj);
    } else if (IS_CLASS_FLAG_SET(obj->clazz, CLASS_ISARRAY)) {
        visitArrayObject(visitor, obj);
    } else {
        visitDataObject(visitor, obj);
    }
    LOGV("Exiting dvmVisitObject(visitor=%p,obj=%p)", visitor, obj);
}
