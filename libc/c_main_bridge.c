/*
 * vkernel userspace - C main bridge
 * Copyright (C) 2026 vkernel authors
 */

int main(int argc, char** argv);

int __vkernel_call_main(int argc, char** argv)
{
    return main(argc, argv);
}
