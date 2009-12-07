#pragma once

#include "Image.h"
#include "Convert.h"
#include "Random.h"
#include "TypeTraits.h"
#include "Point2D.h"
#include "Rectangle.h"
#include "Parallel.h"
#include "Queue.h"
#include "Alpha.h"

namespace IRL
{
    // NNF stands for NearestNeighborField
    template<class PixelType, bool UseSourceMask>
    class NNF
    {
        typedef typename PixelType::DistanceType DistanceType;

    public:
        // Possible types of initial NNF field
        enum InitialFieldType
        {
            RandomField, 
            SmoothField
        };

        Image<PixelType> Source;     // B
        Image<Alpha8>    SourceMask; // which pixel from source is allowed to use
        Image<PixelType> Target;     // A
        Image<Point16> OffsetField; // Result of the algorithm's work
        InitialFieldType InitialOffsetField;

    public:
        NNF();

        // Make one iteration of the algorithm.
        void Iteration(bool parallel = true);
        // Save resulting NNF into file.
        void Save(const std::string& path);

    private:
        // Initializes the algorithm before first iteration.
        void Initialize();
        // Initializes initial NNF with random values
        void RandomFill();
        // Initialized initial NNF with values that arise when interpolate from target to source
        void SmoothFill();

        // If pixel gets in masked zone, tries to move it out of it
        inline void CheckThatNotMasked(int32_t& sx, int32_t& sy);

        // Fills _superPatches vector
        void BuildSuperPatches();
        // Fills _cache variable with initial value
        void PrepareCache(int left, int top, int right, int bottom);

        // Sequential complete iteration over target image's region
        void Iteration(int left, int top, int right, int bottom, int iteration);

        // Perform direct scan order step over target image's region
        void DirectScanOrder(int left, int top, int right, int bottom);
        // Perform reverse scan order step over target image's region
        void ReverseScanOrder(int left, int top, int right, int bottom);

        // Propagation.
        // Direction +1 for direct scan order, -1 for reverse one.
        // LeftAvailable == true if can propagate horizontally.
        // UpAvailable == true if can propagate vertically
        template<int Direction, bool LeftAvailable, bool UpAvailable>
        void Propagate(const Point32& target);

        // Random search step on pixel
        inline void RandomSearch(const Point32& target);

        #pragma region Propagate support methods
        inline DistanceType MoveDistanceByDx(const Point32& target, int dx);
        inline DistanceType MoveDistanceByDy(const Point32& target, int dy);

        template<bool SourceMirroring, bool TargetMirroring>
        inline DistanceType MoveDistanceByDxImpl(int dx, const Point32& target, const Point32& source, DistanceType distance);
        template<bool SourceMirroring, bool TargetMirroring>
        inline DistanceType MoveDistanceByDyImpl(int dy, const Point32& target, const Point32& source, DistanceType distance);
        template<bool SourceMirroring, bool TargetMirroring>
        inline DistanceType MoveDistanceRight(const Point32& target, const Point32& source, DistanceType distance);
        template<bool SourceMirroring, bool TargetMirroring>
        inline DistanceType MoveDistanceLeft(const Point32& target, const Point32& source, DistanceType distance);
        template<bool SourceMirroring, bool TargetMirroring>
        inline DistanceType MoveDistanceUp(const Point32& target, const Point32& source, DistanceType distance);
        template<bool SourceMirroring, bool TargetMirroring>
        inline DistanceType MoveDistanceDown(const Point32& target, const Point32& source, DistanceType distance);

        template<int Direction> bool CheckX(int x); // no implementation here
        template<> inline bool CheckX<-1>(int x) { return x > 0; }
        template<> inline bool CheckX<+1>(int x) { return x < Target.Width() - 1; }

        template<int Direction> bool CheckY(int x);
        template<> inline bool CheckY<-1>(int y) { return y > 0; }
        template<> inline bool CheckY<+1>(int y) { return y < Target.Height() - 1; }
        #pragma endregion

        // Calculate distance from target to source patch.
        // If EarlyTermination == true use 'known' to stop calculation once distance > known
        template<bool EarlyTermination>
        DistanceType Distance(const Point32& targetPatch, const Point32& sourcePatch, DistanceType known = 0);

        // Support method for Distance
        template<bool EarlyTermination, bool SourceMirroring, bool TargetMirroring>
        DistanceType DistanceImpl(const Point32& targetPatch, const Point32& sourcePatch, DistanceType known);

        // Return distance between pixels
        template<bool SourceMirroring, bool TargetMirroring>
        DistanceType PixelDistance(int sx, int sy, int tx, int ty);

        // handy shortcut
        inline Point16& f(const Point32& p) { return OffsetField(p.x, p.y); }

    private:
        // Used to implement multithreading
        // Unit of the thread processing
        struct SuperPatch
        {
            int Left;
            int Top;
            int Right;
            int Bottom;

            SuperPatch* LeftNeighbor;
            SuperPatch* TopNeighbor;
            SuperPatch* RightNeighbor;
            SuperPatch* BottomNeighbor;

            bool AddedToQueue;
            bool Processed;
        };

        // Used to implement multithreading.
        // All tasks share queue with superpatches.
        class IterationTask :
            public Parallel::Runnable
        {
        public:
            IterationTask();
            void Initialize(NNF* owner, Queue<SuperPatch>* queue, int iteration, Mutex* lock);
            virtual void Run();
        private:
            inline void VisitRightPatch(SuperPatch* patch);
            inline void VisitBottomPatch(SuperPatch* patch);
            inline void VisitLeftPatch(SuperPatch* patch);
            inline void VisitTopPatch(SuperPatch* patch);
        private:
            NNF* _owner;
            Queue<SuperPatch>* _queue;
            int _iteration;
            Mutex* _lock;
        };

    private:
        // Holds current best distances
        Image<DistanceType> _cache;
        // Random generator. Note: in parallel mode result is also depends on thread scheduling
        Random _searchRandom;
        // Current iteration number (starts with 0)
        int _iteration;

        // Rectangle with allowed source patch centers
        Rectangle<int32_t> _sourceRect;
        // Rectangle with allowed target patch centers
        Rectangle<int32_t> _targetRect;

        // Rectangle with allowed source patch centers reduced by 1px from all sizes
        Rectangle<int32_t> _sourceRect1px;
        // Rectangle with allowed target patch centers reduced by 1px from all sizes
        Rectangle<int32_t> _targetRect1px;

        // Multithreading support
        Queue<SuperPatch> _superPatchQueue;
        std::vector<SuperPatch> _superPatches;
        SuperPatch* _topLeftSuperPatch;
        SuperPatch* _bottomRightSuperPatch;
        Mutex _lock;
    };
}

#include "NearestNeighborField.inl"