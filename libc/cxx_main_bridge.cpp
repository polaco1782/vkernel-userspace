/*
 * vkernel userspace - C++ main bridge
 * Copyright (C) 2026 vkernel authors
 *
 * Freestanding C++ builds enter user code through this unmangled bridge so
 * application sources can keep a normal C++ main signature.
 */

int __vkernel_cpp_main(int argc, char** argv);

extern "C" int __vkernel_call_main(int argc, char** argv)
{
    return __vkernel_cpp_main(argc, argv);
}
