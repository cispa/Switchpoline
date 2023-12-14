#include <cstdio>
#include <cstdlib>

class Blah {
    public:
        virtual void func(){}
};


class Printer : public Blah {
    public:
        virtual void func(){puts("Hello World!");}
};

class Exiter : public Blah {
    public:
        virtual void func(){exit(0);}
};

int main(int argc, char** argv){
    Blah* blah = (Blah*)new Printer();
    blah->func();
    blah = (Blah*)new Exiter();
    blah->func();
    exit(1337);
}
