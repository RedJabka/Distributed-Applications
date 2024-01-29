# Distributed-Applications
Данный гитхаб проект содержит в себе реализацию 5 лабораторных работ по предмету "Разработка распределенных приложений"

## [Лабораторная работа №1](https://github.com/RedJabka/Distributed-Applications/tree/main/pa1)
В данной лабораторной работе необходимо реализовать библиотеку межпроцессного
взаимодействия посредством обмена сообщениями, которая будет использована в
дальнейшем для изучения основных свойств распределенных вычислительных систем. [Более подробно](https://github.com/RedJabka/Distributed-Applications/blob/main/pa1/README_lab1.md)

## [Лабораторная работа №2](https://github.com/RedJabka/Distributed-Applications/tree/main/pa2)
В данной лабораторной работе необходимо реализовать простую банковскую
систему, использующую физическое время для подсчета полной суммы денег,
находящихся на счетах в разных филиалах банка. В последующих лабораторных работах
будет необходимо модифицировать работу банковской системы для использования
логических часов. [Более подробно](https://github.com/RedJabka/Distributed-Applications/blob/main/pa2/README_lab2.md)

## [Лабораторная работа №3](https://github.com/RedJabka/Distributed-Applications/tree/main/pa3)
В реализации банковской системы из лабораторной работы No2 необходимо
заменить физическое время на скалярное время Лэмпорта. Для обмена отметками времени
процессы должны использовать поле s_local_time структуры MessageHeader, которое
должно содержать показания часов процесса-отправителя на момент отправки сообщения.
Получение текущей отметки времени осуществляется посредством вызова функции
get_lamport_time(), которую необходимо реализовать. [Более подробно](https://github.com/RedJabka/Distributed-Applications/blob/main/pa3/README_lab3.md)

## [Лабораторная работа №4](https://github.com/RedJabka/Distributed-Applications/tree/main/pa4)
Реализация алгоритма взаимного исключения Лэмпорта. [Более подробно](https://github.com/RedJabka/Distributed-Applications/blob/main/pa4/README_lab4.md)

## [Лабораторная работа №5](https://github.com/RedJabka/Distributed-Applications/tree/main/pa5)
Реализация алгоритма взаимного исключения Рикарта-Агравала. [Более подробно](https://github.com/RedJabka/Distributed-Applications/blob/main/pa4/README_lab4.md)

