#pragma once
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
namespace yellow {
class Animal {
  public:
    const std::string Name;
    explicit Animal(std::string name) : Name(std::move(name)) {}
    virtual ~Animal() = default;
};
class Dog : public Animal {
  public:
    explicit Dog(std::string name) : Animal(std::move(name)) {}
    void Bark() const { std::cout << Name << " barks: woof!\n"; }
};
// Deliberately supplied by callers. The checks use local recording fakes, never networks.
void SendSms(const std::string &, const std::string &);
void SendEmail(const std::string &, const std::string &);
class INotifier {
  public:
    virtual ~INotifier() = default;
    virtual void Notify(const std::string &) = 0;
};
class SmsNotifier : public INotifier {
    std::string number_;

  public:
    explicit SmsNotifier(std::string n) : number_(std::move(n)) {}
    void Notify(const std::string &s) override { SendSms(number_, s); }
};
class EmailNotifier : public INotifier {
    std::string email_;

  public:
    explicit EmailNotifier(std::string e) : email_(std::move(e)) {}
    void Notify(const std::string &s) override { SendEmail(email_, s); }
};
class Figure {
  public:
    virtual ~Figure() = default;
    virtual std::string Name() const = 0;
    virtual double Perimeter() const = 0;
    virtual double Area() const = 0;
};
class Rect : public Figure {
    double w_, h_;

  public:
    Rect(double w, double h) : w_(w), h_(h) {}
    std::string Name() const override { return "RECT"; }
    double Perimeter() const override { return 2 * (w_ + h_); }
    double Area() const override { return w_ * h_; }
};
class Triangle : public Figure {
    double a_, b_, c_;

  public:
    Triangle(double a, double b, double c) : a_(a), b_(b), c_(c) {}
    std::string Name() const override { return "TRIANGLE"; }
    double Perimeter() const override { return a_ + b_ + c_; }
    double Area() const override {
        double p = Perimeter() / 2;
        return std::sqrt(p * (p - a_) * (p - b_) * (p - c_));
    }
};
class Circle : public Figure {
    double r_;

  public:
    explicit Circle(double r) : r_(r) {}
    std::string Name() const override { return "CIRCLE"; }
    double Perimeter() const override { return 2 * 3.14 * r_; }
    double Area() const override { return 3.14 * r_ * r_; }
};
inline std::shared_ptr<Figure> CreateFigure(std::istream &in) {
    std::string name;
    double a = 0, b = 0, c = 0;
    in >> name >> a;
    if (name == "RECT") {
        in >> b;
        return std::make_shared<Rect>(a, b);
    }
    if (name == "TRIANGLE") {
        in >> b >> c;
        return std::make_shared<Triangle>(a, b, c);
    }
    if (name == "CIRCLE")
        return std::make_shared<Circle>(a);
    throw std::invalid_argument("unknown figure");
}
namespace refactoring {
class Person {
    std::string role_;

  protected:
    void Say(const std::string &action) const {
        std::cout << role_ << ": " << Name << ' ' << action << '\n';
    }

  public:
    const std::string Name;
    Person(std::string role, std::string name) : role_(std::move(role)), Name(std::move(name)) {}
    virtual ~Person() = default;
    const std::string &Role() const { return role_; }
    virtual void Walk(const std::string &destination) const { Say("walks to: " + destination); }
};
class Student : public Person {
    std::string song_;

  public:
    Student(std::string name, std::string song)
        : Person("Student", std::move(name)), song_(std::move(song)) {}
    void Learn() const { Say("learns"); }
    void SingSong() const { Say("sings a song: " + song_); }
    void Walk(const std::string &destination) const override {
        Person::Walk(destination);
        SingSong();
    }
};
class Teacher : public Person {
    std::string subject_;

  public:
    Teacher(std::string n, std::string s)
        : Person("Teacher", std::move(n)), subject_(std::move(s)) {}
    void Teach() const { Say("teaches: " + subject_); }
};
class Policeman : public Person {
  public:
    explicit Policeman(std::string n) : Person("Policeman", std::move(n)) {}
    void Check(const Person &p) const {
        Say("checks " + p.Role() + ". " + p.Role() + "'s name is: " + p.Name);
    }
};
inline void VisitPlaces(const Person &person, const std::vector<std::string> &places) {
    for (const auto &place : places)
        person.Walk(place);
}
} // namespace refactoring
} // namespace yellow
