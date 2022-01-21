#include "jvm.h"

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "class_file.h"
#include "heap.h"
#include "read_class.h"

/** The name of the method to invoke to run the class file */
const char MAIN_METHOD[] = "main";
/**
 * The "descriptor" string for main(). The descriptor encodes main()'s signature,
 * i.e. main() takes a String[] and returns void.
 * If you're interested, the descriptor string is explained at
 * https://docs.oracle.com/javase/specs/jvms/se12/html/jvms-4.html#jvms-4.3.2.
 */
const char MAIN_DESCRIPTOR[] = "([Ljava/lang/String;)V";

/**
 * Represents the return value of a Java method: either void or an int or a reference.
 * For simplification, we represent a reference as an index into a heap-allocated array.
 * (In a real JVM, methods could also return object references or other primitives.)
 */
typedef struct {
    /** Whether this returned value is an int */
    bool has_value;
    /** The returned value (only valid if `has_value` is true) */
    int32_t value;
} optional_value_t;

/**
 * Runs a method's instructions until the method returns.
 *
 * @param method the method to run
 * @param locals the array of local variables, including the method parameters.
 *   Except for parameters, the locals are uninitialized.
 * @param class the class file the method belongs to
 * @param heap an array of heap-allocated pointers, useful for references
 * @return an optional int containing the method's return value
 */
optional_value_t execute(method_t *method, int32_t *locals, class_file_t *class,
                         heap_t *heap) {
    code_t more_code = method->code;
    u1 *bytes = more_code.code;
    int32_t *stack = calloc(sizeof(int32_t), more_code.max_stack);
    size_t pc = 0; // counter
    size_t index = 0;
    while (pc < more_code.code_length) {
        u1 instruc = bytes[pc];
        if (instruc == i_bipush) {
            pc++;
            int8_t a = bytes[pc];
            stack[index] = (int32_t) a;
            index++;
            pc++;
        }
        else if (instruc >= i_iadd && instruc <= i_ixor && instruc != i_ineg) {
            pc++;
            assert(index >= 2);
            int32_t a = stack[index - 2];
            int32_t b = stack[index - 1];
            int32_t s = 0;
            if (instruc == i_iadd) {
                s = a + b;
            }
            if (instruc == i_isub) {
                s = a - b;
            }
            if (instruc == i_imul) {
                s = a * b;
            }
            if (instruc == i_idiv) {
                assert(b != 0);
                s = a / b;
            }
            if (instruc == i_irem) {
                assert(b != 0);
                s = a % b;
            }
            if (instruc == i_ishl) {
                s = a << b;
            }
            if (instruc == i_ishr) {
                s = a >> b;
            }
            if (instruc == i_iushr) {
                s = ((unsigned) a) >> b;
            }
            if (instruc == i_iand) {
                s = a & b;
            }
            if (instruc == i_ior) {
                s = a | b;
            }
            if (instruc == i_ixor) {
                s = a ^ b;
            }
            stack[index - 2] = s;
            index--;
        }
        else if (instruc == i_ineg) {
            pc++;
            assert(index >= 1);
            int32_t a = stack[index - 1];
            int32_t s = a * -1;
            stack[index - 1] = s;
        }
        else if (instruc == i_return) {
            pc++;
            break;
        }
        else if (instruc == i_getstatic) {
            pc += 3;
        }
        else if (instruc == i_invokevirtual) {
            if (index > 0) {
                printf("%i\n", stack[index - 1]);
                index--;
                pc += 3;
            }
        }
        else if (instruc >= i_iconst_m1 && instruc <= i_iconst_5) {
            stack[index] = instruc - i_iconst_0;
            index++;
            pc++;
        }
        else if (instruc == i_sipush) {
            pc++;
            int16_t b1 = bytes[pc];
            pc++;
            int16_t b2 = bytes[pc];
            stack[index] = (int16_t)((b1 << 8) | b2);
            index++;
            pc++;
        }
        else if (instruc == i_iload || instruc == i_aload) {
            pc++;
            stack[index] = locals[bytes[pc]];
            index++;
            pc++;
        }
        else if (instruc == i_istore) {
            assert(index >= 1);
            if (index >= 1) {
                pc++;
                index--;
                locals[bytes[pc]] = stack[index];
                pc++;
            }
        }
        else if (instruc == i_iinc) {
            pc++;
            uint8_t i = bytes[pc];
            pc++;
            int8_t b = bytes[pc];
            locals[i] += b;
            pc++;
        }
        else if (instruc >= i_iload_0 && instruc <= i_iload_3) {
            stack[index] = locals[instruc - i_iload_0];
            index++;
            pc++;
        }
        else if (instruc >= i_istore_0 && instruc <= i_istore_3) {
            assert(index >= 1);
            if (index >= 1) {
                index--;
                locals[instruc - i_istore_0] = stack[index];
                pc++;
            }
        }
        else if (instruc == i_ldc) {
            pc++;
            uint8_t b = bytes[pc];
            stack[index] = *(int32_t *) class->constant_pool[b - 1].info;
            index++;
            pc++;
        }
        else if (instruc == i_ifeq) {
            assert(index >= 1);
            pc++;
            index--;
            int32_t a = stack[index];
            int8_t b1 = bytes[pc];
            pc++;
            int8_t b2 = bytes[pc];
            if (a == 0) {
                pc += ((b1 << 8) | b2) - 2;
            }
            else {
                pc++;
            }
        }
        else if (instruc == i_ifne) {
            assert(index >= 1);
            pc++;
            index--;
            int32_t a = stack[index];
            int8_t b1 = bytes[pc];
            pc++;
            int8_t b2 = bytes[pc];
            if (a != 0) {
                pc += ((b1 << 8) | b2) - 2;
            }
            else {
                pc++;
            }
        }
        else if (instruc == i_iflt) {
            assert(index >= 1);
            pc++;
            index--;
            int32_t a = stack[index];
            int8_t b1 = bytes[pc];
            pc++;
            int8_t b2 = bytes[pc];
            if (a < 0) {
                pc += ((b1 << 8) | b2) - 2;
            }
            else {
                pc++;
            }
        }
        else if (instruc == i_ifge) {
            assert(index >= 1);
            pc++;
            index--;
            int32_t a = stack[index];
            int8_t b1 = bytes[pc];
            pc++;
            int8_t b2 = bytes[pc];
            if (a >= 0) {
                pc += ((b1 << 8) | b2) - 2;
            }
            else {
                pc++;
            }
        }
        else if (instruc == i_ifgt) {
            assert(index >= 1);
            pc++;
            index--;
            int32_t a = stack[index];
            int8_t b1 = bytes[pc];
            pc++;
            int8_t b2 = bytes[pc];
            if (a > 0) {
                pc += ((b1 << 8) | b2) - 2;
            }
            else {
                pc++;
            }
        }
        else if (instruc == i_ifle) {
            assert(index >= 1);
            pc++;
            index--;
            int32_t a = stack[index];
            int8_t b1 = bytes[pc];
            pc++;
            int8_t b2 = bytes[pc];
            if (a <= 0) {
                pc += ((b1 << 8) | b2) - 2;
            }
            else {
                pc++;
            }
        }
        else if (instruc == i_if_icmpeq) {
            assert(index >= 2);
            pc++;
            index--;
            int32_t b = stack[index];
            index--;
            int32_t a = stack[index];
            int8_t b1 = bytes[pc];
            pc++;
            int8_t b2 = bytes[pc];
            if (a == b) {
                pc += ((b1 << 8) | b2) - 2;
            }
            else {
                pc++;
            }
        }
        else if (instruc == i_if_icmpne) {
            assert(index >= 2);
            pc++;
            index--;
            int32_t b = stack[index];
            index--;
            int32_t a = stack[index];
            int8_t b1 = bytes[pc];
            pc++;
            int8_t b2 = bytes[pc];
            if (a != b) {
                pc += ((b1 << 8) | b2) - 2;
            }
            else {
                pc++;
            }
        }
        else if (instruc == i_if_icmplt) {
            assert(index >= 2);
            pc++;
            index--;
            int32_t b = stack[index];
            index--;
            int32_t a = stack[index];
            int8_t b1 = bytes[pc];
            pc++;
            int8_t b2 = bytes[pc];
            if (a < b) {
                pc += ((b1 << 8) | b2) - 2;
            }
            else {
                pc++;
            }
        }
        else if (instruc == i_if_icmpge) {
            assert(index >= 2);
            pc++;
            index--;
            int32_t b = stack[index];
            index--;
            int32_t a = stack[index];
            int8_t b1 = bytes[pc];
            pc++;
            int8_t b2 = bytes[pc];
            if (a >= b) {
                pc += ((b1 << 8) | b2) - 2;
            }
            else {
                pc++;
            }
        }
        else if (instruc == i_if_icmpgt) {
            assert(index >= 2);
            pc++;
            index--;
            int32_t b = stack[index];
            index--;
            int32_t a = stack[index];
            int8_t b1 = bytes[pc];
            pc++;
            int8_t b2 = bytes[pc];
            if (a > b) {
                pc += ((b1 << 8) | b2) - 2;
            }
            else {
                pc++;
            }
        }
        else if (instruc == i_if_icmple) {
            assert(index >= 2);
            pc++;
            index--;
            int32_t b = stack[index];
            index--;
            int32_t a = stack[index];
            int8_t b1 = bytes[pc];
            pc++;
            int8_t b2 = bytes[pc];
            if (a <= b) {
                pc += ((b1 << 8) | b2) - 2;
            }
            else {
                pc++;
            }
        }
        else if (instruc == i_goto) {
            pc++;
            int8_t b1 = bytes[pc];
            pc++;
            int8_t b2 = bytes[pc];
            pc += ((b1 << 8) | b2) - 2;
        }
        else if (instruc == i_ireturn) {
            assert(index >= 1);
            index--;
            optional_value_t result = {.has_value = true, .value = stack[index]};
            free(stack);
            return result;
        }
        else if (instruc == i_invokestatic) {
            pc++;
            uint16_t b1 = bytes[pc];
            pc++;
            uint16_t b2 = bytes[pc];
            method_t *m = find_method_from_index((b1 << 8) | b2, class);
            uint16_t params = get_number_of_parameters(m);
            int32_t locals_arr[m->code.max_locals];
            for (size_t i = 0; i < params; i++) {
                index--;
                locals_arr[params - i - 1] = stack[index];
            }
            optional_value_t result = execute(m, locals_arr, class, heap);
            if (result.has_value == true) {
                stack[index] = result.value;
                index++;
            }
            pc++;
        }
        else if (instruc == i_nop) {
            pc++;
        }
        else if (instruc == i_dup) {
            assert(index >= 1);
            stack[index] = stack[index - 1];
            index++;
            pc++;
        }
        else if (instruc == i_newarray) {
            assert(index >= 1);
            int32_t *new_array = calloc(sizeof(int32_t), stack[index - 1] + 1);
            new_array[0] = stack[index - 1];
            stack[index - 1] = heap_add(heap, new_array);
            pc += 2;
        }
        else if (instruc == i_arraylength) {
            assert(index >= 1);
            int32_t *arr = heap_get(heap, stack[index - 1]);
            stack[index - 1] = arr[0];
            pc++;
        }
        else if (instruc == i_areturn) {
            assert(index >= 1);
            index--;
            optional_value_t result = {.has_value = true, .value = stack[index]};
            pc++;
            free(stack);
            return result;
        }
        else if (instruc == i_iastore) {
            assert(index >= 3);
            index--;
            int32_t value = stack[index];
            index--;
            int32_t heap_idx = stack[index];
            index--;
            int32_t ref = stack[index];
            int32_t *arr = heap_get(heap, ref);
            arr[heap_idx + 1] = value;
            pc++;
        }
        else if (instruc == i_iaload) {
            assert(index >= 2);
            index--;
            int32_t heap_idx = stack[index];
            index--;
            int32_t ref = stack[index];
            int32_t *arr = heap_get(heap, ref);
            int32_t value = arr[heap_idx + 1];
            stack[index] = value;
            index++;
            pc++;
        }
        else if (instruc == i_astore) {
            assert(index >= 1);
            if (index >= 1) {
                index--;
                pc++;
                locals[bytes[pc]] = stack[index];
                pc++;
            }
        }
        else if (instruc >= i_aload_0 && instruc <= i_aload_3) {
            stack[index] = locals[instruc - i_aload_0];
            index++;
            pc++;
        }
        else if (instruc >= i_astore_0 && instruc <= i_astore_3) {
            assert(index >= 1);
            if (index >= 1) {
                index--;
                locals[instruc - i_astore_0] = stack[index];
                pc++;
            }
        }
    }
    free(stack);
    // Return void
    optional_value_t result = {.has_value = false};
    return result;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "USAGE: %s <class file>\n", argv[0]);
        return 1;
    }

    // Open the class file for reading
    FILE *class_file = fopen(argv[1], "r");
    assert(class_file != NULL && "Failed to open file");

    // Parse the class file
    class_file_t *class = get_class(class_file);
    int error = fclose(class_file);
    assert(error == 0 && "Failed to close file");

    // The heap array is initially allocated to hold zero elements.
    heap_t *heap = heap_init();

    // Execute the main method
    method_t *main_method = find_method(MAIN_METHOD, MAIN_DESCRIPTOR, class);
    assert(main_method != NULL && "Missing main() method");
    /* In a real JVM, locals[0] would contain a reference to String[] args.
     * But since TeenyJVM doesn't support Objects, we leave it uninitialized. */
    int32_t locals[main_method->code.max_locals];
    // Initialize all local variables to 0
    memset(locals, 0, sizeof(locals));
    optional_value_t result = execute(main_method, locals, class, heap);
    assert(!result.has_value && "main() should return void");

    // Free the internal data structures
    free_class(class);

    // Free the heap
    heap_free(heap);
}
