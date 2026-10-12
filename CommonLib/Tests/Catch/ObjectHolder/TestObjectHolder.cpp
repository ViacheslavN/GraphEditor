#include "TestCommon.h"

using namespace CommonLib;
using namespace test_objects;

// ---------------------------------------------------------------------------
// AddObject / GetObjectByHandle
// ---------------------------------------------------------------------------

TEST_CASE("Empty holder", "[objectholder]")
{
	CObjectHolder holder;

	REQUIRE(holder.GetCount() == 0);
	REQUIRE_FALSE(holder.IsExist(0));
	REQUIRE_FALSE(holder.IsExist(12345));
}

TEST_CASE("Added object can be got back by its handle", "[objectholder]")
{
	CObjectHolder holder;
	CRectanglePtr ptrRect = std::make_shared<CRectangle>(2., 3.);

	CObjectHolder::ObjectHandle handle = holder.AddObject(ptrRect);

	REQUIRE(handle != 0);
	REQUIRE(handle == reinterpret_cast<CObjectHolder::ObjectHandle>(ptrRect.get()));
	REQUIRE(holder.IsExist(handle));
	REQUIRE(holder.GetCount() == 1);

	CRectanglePtr ptrGot = holder.GetObjectByHandle<CRectangle>(handle);
	REQUIRE(ptrGot == ptrRect);
	REQUIRE(ptrGot->GetArea() == Catch::Approx(6.));
}

TEST_CASE("Object added through an interface is got back through the same interface", "[objectholder]")
{
	CObjectHolder holder;
	IShapePtr ptrCircle = std::make_shared<CCircle>(1.);
	IShapePtr ptrRect   = std::make_shared<CRectangle>(4., 5.);

	CObjectHolder::ObjectHandle hCircle = holder.AddObject(ptrCircle);
	CObjectHolder::ObjectHandle hRect   = holder.AddObject(ptrRect);

	REQUIRE(hCircle != hRect);
	REQUIRE(holder.GetCount() == 2);

	IShapePtr ptrShape1 = holder.GetObjectByHandle<IShape>(hCircle);
	IShapePtr ptrShape2 = holder.GetObjectByHandle<IShape>(hRect);

	REQUIRE(ptrShape1->GetName() == "Circle");
	REQUIRE(ptrShape1->GetArea() == Catch::Approx(3.141592653589793));
	REQUIRE(ptrShape2->GetName() == "Rectangle");
	REQUIRE(ptrShape2->GetArea() == Catch::Approx(20.));

	// the concrete object behind the interface is reachable with a cast
	CRectanglePtr ptrConcrete = std::dynamic_pointer_cast<CRectangle>(ptrShape2);
	REQUIRE(ptrConcrete != nullptr);
}

TEST_CASE("Changes made through a got object are visible to the owner", "[objectholder]")
{
	CObjectHolder holder;
	CRectanglePtr ptrRect = std::make_shared<CRectangle>(1., 1.);
	IMovablePtr   ptrMovable = ptrRect;

	CObjectHolder::ObjectHandle handle = holder.AddObject(ptrMovable);
	holder.GetObjectByHandle<IMovable>(handle)->Move(10., -5.);

	REQUIRE(ptrRect->GetX() == Catch::Approx(10.));
	REQUIRE(ptrRect->GetY() == Catch::Approx(-5.));
}

TEST_CASE("GetObjectByHandle requires exactly the type the object was added with", "[objectholder]")
{
	CObjectHolder holder;
	CRectanglePtr ptrRect = std::make_shared<CRectangle>(2., 2.);
	CObjectHolder::ObjectHandle handle = holder.AddObject(ptrRect);

	// stored as shared_ptr<CRectangle>: neither its interfaces nor an unrelated class match
	REQUIRE_THROWS_AS(holder.GetObjectByHandle<IShape>(handle), CExcBase);
	REQUIRE_THROWS_AS(holder.GetObjectByHandle<IMovable>(handle), CExcBase);
	REQUIRE_THROWS_AS(holder.GetObjectByHandle<CCircle>(handle), CExcBase);

	// the failed lookups do not change the holder
	REQUIRE(holder.IsExist(handle));
	REQUIRE(holder.GetObjectByHandle<CRectangle>(handle) == ptrRect);
}

TEST_CASE("Adding a null object throws", "[objectholder]")
{
	CObjectHolder holder;

	REQUIRE_THROWS_AS(holder.AddObject(IShapePtr()), CExcBase);
	REQUIRE_THROWS_AS(holder.AddObject(CRectanglePtr()), CExcBase);
	REQUIRE(holder.GetCount() == 0);
}

TEST_CASE("Adding an already registered object throws", "[objectholder]")
{
	CObjectHolder holder;
	CRectanglePtr ptrRect = std::make_shared<CRectangle>(3., 3.);

	CObjectHolder::ObjectHandle handle = holder.AddObject(ptrRect);

	REQUIRE_THROWS_AS(holder.AddObject(ptrRect), CExcBase);
	REQUIRE(holder.GetCount() == 1);

	// the originally registered object is untouched
	REQUIRE(holder.GetObjectByHandle<CRectangle>(handle) == ptrRect);

	SECTION("also when the same pointer is passed through the same interface type")
	{
		IShapePtr ptrShape1 = std::make_shared<CCircle>(1.);
		IShapePtr ptrShape2 = ptrShape1;

		holder.AddObject(ptrShape1);
		REQUIRE_THROWS_AS(holder.AddObject(ptrShape2), CExcBase);
		REQUIRE(holder.GetCount() == 2);
	}

	SECTION("also when the object pointer is the same but the static type differs")
	{
		// IShape is the first base of CRectangle, so IShape* == CRectangle*
		IShapePtr ptrShape = ptrRect;
		REQUIRE(static_cast<void*>(ptrShape.get()) == static_cast<void*>(ptrRect.get()));
		REQUIRE_THROWS_AS(holder.AddObject(ptrShape), CExcBase);
		REQUIRE(holder.GetCount() == 1);
	}
}

TEST_CASE("The same object through a second interface gets a different handle", "[objectholder]")
{
	// The handle is the address of the pointer passed in. With multiple inheritance
	// IMovable* points into the middle of CRectangle, so it is a different handle and is
	// not detected as a duplicate.
	CObjectHolder holder;
	CRectanglePtr ptrRect = std::make_shared<CRectangle>(1., 2.);
	IShapePtr     ptrShape = ptrRect;
	IMovablePtr   ptrMovable = ptrRect;

	CObjectHolder::ObjectHandle hShape = holder.AddObject(ptrShape);
	CObjectHolder::ObjectHandle hMovable = holder.AddObject(ptrMovable);

	REQUIRE(hShape != hMovable);
	REQUIRE(holder.GetCount() == 2);
	REQUIRE(holder.GetObjectByHandle<IShape>(hShape)->GetName() == "Rectangle");
	REQUIRE(holder.GetObjectByHandle<IMovable>(hMovable)->GetX() == Catch::Approx(0.));
}

// ---------------------------------------------------------------------------
// RemoveObject / lifetime
// ---------------------------------------------------------------------------

TEST_CASE("Removed object is not accessible anymore", "[objectholder]")
{
	CObjectHolder holder;
	IShapePtr ptrCircle = std::make_shared<CCircle>(2.);
	IShapePtr ptrRect   = std::make_shared<CRectangle>(1., 1.);

	CObjectHolder::ObjectHandle hCircle = holder.AddObject(ptrCircle);
	CObjectHolder::ObjectHandle hRect   = holder.AddObject(ptrRect);

	holder.RemoveObject(hCircle);

	REQUIRE_FALSE(holder.IsExist(hCircle));
	REQUIRE(holder.GetCount() == 1);
	REQUIRE_THROWS_AS(holder.GetObjectByHandle<IShape>(hCircle), CExcBase);

	// the other object is untouched
	REQUIRE(holder.IsExist(hRect));
	REQUIRE(holder.GetObjectByHandle<IShape>(hRect) == ptrRect);
}

TEST_CASE("Removing an unknown or already removed handle throws", "[objectholder]")
{
	CObjectHolder holder;
	CRectanglePtr ptrRect = std::make_shared<CRectangle>(1., 1.);

	REQUIRE_THROWS_AS(holder.RemoveObject(0), CExcBase);
	REQUIRE_THROWS_AS(holder.RemoveObject(reinterpret_cast<CObjectHolder::ObjectHandle>(ptrRect.get())), CExcBase);

	CObjectHolder::ObjectHandle handle = holder.AddObject(ptrRect);
	holder.RemoveObject(handle);

	REQUIRE_THROWS_AS(holder.RemoveObject(handle), CExcBase);
	REQUIRE(holder.GetCount() == 0);
}

TEST_CASE("Getting an unknown handle throws", "[objectholder]")
{
	CObjectHolder holder;
	holder.AddObject(std::make_shared<CCircle>(1.));

	REQUIRE_THROWS_AS(holder.GetObjectByHandle<CCircle>(0), CExcBase);
	REQUIRE_THROWS_AS(holder.GetObjectByHandle<CCircle>(1), CExcBase);
}

TEST_CASE("Object can be registered again after it was removed", "[objectholder]")
{
	CObjectHolder holder;
	CRectanglePtr ptrRect = std::make_shared<CRectangle>(1., 1.);

	CObjectHolder::ObjectHandle handle1 = holder.AddObject(ptrRect);
	holder.RemoveObject(handle1);
	CObjectHolder::ObjectHandle handle2 = holder.AddObject(ptrRect);

	REQUIRE(handle1 == handle2);
	REQUIRE(holder.GetObjectByHandle<CRectangle>(handle2) == ptrRect);
}

TEST_CASE("Holder keeps the object alive until it is removed", "[objectholder][lifetime]")
{
	const int liveBefore = LiveObjects();
	CObjectHolder holder;
	CObjectHolder::ObjectHandle handle = 0;

	{
		IShapePtr ptrCircle = std::make_shared<CCircle>(1.);
		handle = holder.AddObject(ptrCircle);
		REQUIRE(ptrCircle.use_count() == 2);
	}

	// the caller's pointer is gone, the holder still owns the object
	REQUIRE(LiveObjects() == liveBefore + 1);
	REQUIRE(holder.GetObjectByHandle<IShape>(handle)->GetName() == "Circle");

	holder.RemoveObject(handle);
	REQUIRE(LiveObjects() == liveBefore);
}

TEST_CASE("Object got before removing stays valid after removing", "[objectholder][lifetime]")
{
	const int liveBefore = LiveObjects();
	CObjectHolder holder;

	CObjectHolder::ObjectHandle handle = holder.AddObject(IShapePtr(std::make_shared<CRectangle>(2., 5.)));
	IShapePtr ptrGot = holder.GetObjectByHandle<IShape>(handle);

	holder.RemoveObject(handle);

	REQUIRE(LiveObjects() == liveBefore + 1);
	REQUIRE(ptrGot->GetArea() == Catch::Approx(10.));

	ptrGot.reset();
	REQUIRE(LiveObjects() == liveBefore);
}

TEST_CASE("Destroying the holder releases all objects", "[objectholder][lifetime]")
{
	const int liveBefore = LiveObjects();
	{
		CObjectHolder holder;
		holder.AddObject(std::make_shared<CCircle>(1.));
		holder.AddObject(std::make_shared<CRectangle>(1., 1.));
		holder.AddObject(IShapePtr(std::make_shared<CCircle>(2.)));
		REQUIRE(LiveObjects() == liveBefore + 3);
	}
	REQUIRE(LiveObjects() == liveBefore);
}

// ---------------------------------------------------------------------------
// Threads
// ---------------------------------------------------------------------------

TEST_CASE("Concurrent add, get and remove from several threads", "[objectholder][threads]")
{
	const int liveBefore = LiveObjects();
	const int threadCount = 8;
	const int objectsPerThread = 500;

	CObjectHolder holder;
	std::atomic<int> errors{0};
	std::vector<std::thread> threads;

	for (int t = 0; t < threadCount; ++t)
	{
		threads.emplace_back([&holder, &errors, t, objectsPerThread]()
		{
			try
			{
				std::vector<CObjectHolder::ObjectHandle> handles;
				for (int i = 0; i < objectsPerThread; ++i)
				{
					if ((i + t) % 2 == 0)
						handles.push_back(holder.AddObject(IShapePtr(std::make_shared<CCircle>(1. + i))));
					else
						handles.push_back(holder.AddObject(IShapePtr(std::make_shared<CRectangle>(1. + i, 2.))));
				}

				for (CObjectHolder::ObjectHandle handle : handles)
				{
					if (holder.GetObjectByHandle<IShape>(handle)->GetArea() <= 0.)
						++errors;
				}

				for (CObjectHolder::ObjectHandle handle : handles)
					holder.RemoveObject(handle);
			}
			catch (...)
			{
				++errors;
			}
		});
	}

	for (std::thread& thread : threads)
		thread.join();

	REQUIRE(errors == 0);
	REQUIRE(holder.GetCount() == 0);
	REQUIRE(LiveObjects() == liveBefore);
}
