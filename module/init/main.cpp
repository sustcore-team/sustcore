class A {
public:
    int a;
    A() : a(0) {}
};

A b;

extern "C" int kmain() {
    if (b.a != 0) {
        return -1;
    }
    return 0;
}
