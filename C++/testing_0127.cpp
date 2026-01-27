#include <iostream>

namespace std1
{
    int A = 10;
    int B = 20;
    void printMessage()
    {
        std::cout << "Hello from namespace std1!" << std::endl;
    }
}

int main()
{
    std::cout << "Hello, World!" << std::endl;  //std::是命名空间，cout是标准输出流对象，endl是换行符
    
    using std1::A; // 使用命名空间std1中的A变量
    //如果调用命名空间内的变量应该怎么做？？
    // 直接使用A即可
    std::cout << "Value of A: " << A << std::endl;
    return 0;
}