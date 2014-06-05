#ifndef PRINT_HPP
#define PRINT_HPP

#include <iostream>

inline void print_raw()
{}

template <typename T, typename... Ts>
void print_raw(T&& t, Ts&&... ts)
{
    std::cout << t;
    print_raw(ts...);
}

template <typename... Ts>
void print(Ts&&... ts)
{
    print_raw(ts...);
    std::cout << std::endl;
}

#endif // PRINT_HPP
