#ifndef COMPACTRPC_COCTX_H
#define COMPACTRPC_COCTX_H

namespace compactrpc
{

    enum
    {
        KRBP = 6,     // rbp
        KRDI = 7,     // rdi, ��һ����������
        KRSI = 8,     // rsi, �ڶ�����������
        KRETAddr = 9, // ������ת�󼴽�Ҫִ�е�ָ���ַ�� ��rip��ֵ
        KRSP = 13,    // rsp
    };
    struct coctx
    {
        void *regs[14];
    };
    extern "C"
    {
        //
        extern void coctx_swap(coctx *, coctx *) asm("coctx_swap");
    };
}
#endif