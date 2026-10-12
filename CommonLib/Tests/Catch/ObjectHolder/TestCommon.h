#pragma once

// Shared includes and test types for the CommonLib/ObjectHolder Catch2 tests.

#ifdef _WIN32
#define NOMINMAX
#endif

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "CommonLib/CommonLib.h"
#include "CommonLib/exception/exc_base.h"
#include "CommonLib/ObjectHolder/ObjectHolder.h"

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace test_objects
{
	// Number of live test objects, to check that the holder releases its references.
	inline std::atomic<int>& LiveObjects()
	{
		static std::atomic<int> count{0};
		return count;
	}

	class IShape
	{
	public:
		virtual ~IShape() = default;
		virtual double      GetArea() const = 0;
		virtual std::string GetName() const = 0;
	};

	class IMovable
	{
	public:
		virtual ~IMovable() = default;
		virtual void   Move(double dx, double dy) = 0;
		virtual double GetX() const = 0;
		virtual double GetY() const = 0;
	};

	typedef std::shared_ptr<IShape>   IShapePtr;
	typedef std::shared_ptr<IMovable> IMovablePtr;

	// Implements both interfaces (multiple inheritance: IShape* and IMovable* of the
	// same object have different addresses).
	class CRectangle : public IShape, public IMovable
	{
	public:
		CRectangle(double width, double height) : m_width(width), m_height(height) { ++LiveObjects(); }
		~CRectangle() override { --LiveObjects(); }

		double      GetArea() const override { return m_width * m_height; }
		std::string GetName() const override { return "Rectangle"; }

		void   Move(double dx, double dy) override { m_x += dx; m_y += dy; }
		double GetX() const override { return m_x; }
		double GetY() const override { return m_y; }

	private:
		double m_width;
		double m_height;
		double m_x = 0.;
		double m_y = 0.;
	};

	class CCircle : public IShape
	{
	public:
		explicit CCircle(double radius) : m_radius(radius) { ++LiveObjects(); }
		~CCircle() override { --LiveObjects(); }

		double      GetArea() const override { return 3.141592653589793 * m_radius * m_radius; }
		std::string GetName() const override { return "Circle"; }

	private:
		double m_radius;
	};

	typedef std::shared_ptr<CRectangle> CRectanglePtr;
	typedef std::shared_ptr<CCircle>    CCirclePtr;
}
