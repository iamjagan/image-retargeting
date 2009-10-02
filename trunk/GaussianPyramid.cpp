#include "GaussianPyramid.h"
#include "Parallel.h"
#include "Profiler.h"

#include <sstream>

namespace IRL
{
    template<class Kernel>
    const Image ScaleDown(const Image& img);

    class Kernel1D5Tap
    {
        static int LookUp[5];
    public:
        static const int HalfSize()   { return 2; }
        static const int Value(int i) { return LookUp[i + HalfSize()]; }
        static const int Sum()        { return 16; }
    };
    int Kernel1D5Tap::LookUp[5] = { 1, 4, 6, 4, 1 };

    GaussianPyramid::Private::Private(const Image& image, int levels)
    {
        Tools::Profiler profiler("GaussianPyramid::Private::Private");
        ASSERT(levels > 0);
        Levels.resize(levels);
        Levels[0] = image;
        for (int i = 1; i < levels; i++)
            Levels[i] = ScaleDown<Kernel1D5Tap>(Levels[i - 1]);
    }

    void GaussianPyramid::Private::Save(const std::string& filePath)
    {
        std::string prefix;
        std::string ext;
        int dot = filePath.find_last_of('.');
        if (dot != -1)
        {
            prefix = filePath.substr(0, dot);
            ext = filePath.substr(dot, filePath.size() - dot);
        } else
            prefix = filePath;

        for (unsigned int i = 0; i < Levels.size(); i++)
        {
            std::ostringstream fullPath;
            fullPath << prefix << "_" << i << ext;
            Levels[i].Save(fullPath.str());
        }
    }

    //////////////////////////////////////////////////////////////////////////
    // Scaling routines

    class ScalingTask :
        public Parallel::Runnable
    {
    protected:
        const Image* Src;
        Image* Dst;
        int StartPos;
        int StopPos;
    public:
        void Set(const Image& src, Image& dst, int startPos, int stopPos)
        {
            Src = &src;
            Dst = &dst;
            StartPos = startPos;
            StopPos = stopPos;
        }
    };

    template<class Kernel>
    class HScalingTask :
        public ScalingTask
    {
    public:
        virtual void Run()
        {
            for (int y = StartPos; y < StopPos; y++)
                ProcessLine(y);
        }

        inline void ProcessLine(int y)
        {
            const int EdgeSize = Kernel::HalfSize() / 2;
            const int Width = Dst->Width();
            int x = 0;
            for (; x < EdgeSize; x++)
                Dst->Pixel(x, y) = ProcessLeftEdge(x, y);
            for (; x < Width - EdgeSize; x++)
                Dst->Pixel(x, y) = ProcessMidlePart(x, y);
            for (; x < Width; x++)
                Dst->Pixel(x, y) = ProcessRightEdge(x, y);
        }

        inline Color ProcessLeftEdge(int x, int y)
        {
            int R = 0;
            int G = 0;
            int B = 0;
            for (int m = -Kernel::HalfSize(); m <= Kernel::HalfSize(); m++)
            {
                const Color* src = &Src->Pixel(abs(2*x + m), 2*y);
                R += int(src->R) * Kernel::Value(m);
                G += int(src->G) * Kernel::Value(m);
                B += int(src->B) * Kernel::Value(m);
            }
            R /= Kernel::Sum();
            G /= Kernel::Sum();
            B /= Kernel::Sum();
            return Color(R, G, B, Src->Pixel(2*x, 2*y).A);
        }

        inline Color ProcessMidlePart(int x, int y)
        {
            int R = 0;
            int G = 0;
            int B = 0;
            for (int m = -Kernel::HalfSize(); m <= Kernel::HalfSize(); m++)
            {
                const Color* src = &Src->Pixel(2*x + m, 2*y);
                R += int(src->R) * Kernel::Value(m);
                G += int(src->G) * Kernel::Value(m);
                B += int(src->B) * Kernel::Value(m);
            }
            R /= Kernel::Sum();
            G /= Kernel::Sum();
            B /= Kernel::Sum();
            return Color(R, G, B, Src->Pixel(2*x, 2*y).A);
        }

        inline Color ProcessRightEdge(int x, int y)
        {
            int R = 0;
            int G = 0;
            int B = 0;
            const int maxX = Src->Width() - 1;
            for (int m = -Kernel::HalfSize(); m <= Kernel::HalfSize(); m++)
            {
                const Color* src = &Src->Pixel(maxX - abs(maxX - (2*x + m)), 2 * y);
                R += int(src->R) * Kernel::Value(m);
                G += int(src->G) * Kernel::Value(m);
                B += int(src->B) * Kernel::Value(m);
            }
            R /= Kernel::Sum();
            G /= Kernel::Sum();
            B /= Kernel::Sum();
            return Color(R, G, B, Src->Pixel(2*x, 2*y).A);
        }
    };

    template<class Kernel>
    class VScalingTask :
        public ScalingTask
    {
    public:
        virtual void Run()
        {
            for (int x = StartPos; x < StopPos; x++)
                ProcessLine(x);
        }

        inline void ProcessLine(int x)
        {
            const int EdgeSize = Kernel::HalfSize() / 2;
            const int Height = Dst->Height();
            int y = 0;
            for (; y < EdgeSize; y++)
                Dst->Pixel(x, y) = ProcessUpEdge(x, y);
            for (; y < Height - EdgeSize; y++)
                Dst->Pixel(x, y) = ProcessMidlePart(x, y);
            for (; y < Height; y++)
                Dst->Pixel(x, y) = ProcessDownEdge(x, y);
        }

        inline Color ProcessUpEdge(int x, int y)
        {
            int R = 0;
            int G = 0;
            int B = 0;
            for (int m = -Kernel::HalfSize(); m <= Kernel::HalfSize(); m++)
            {
                const Color* src = &Src->Pixel(2*x, abs(2*y + m));
                R += int(src->R) * Kernel::Value(m);
                G += int(src->G) * Kernel::Value(m);
                B += int(src->B) * Kernel::Value(m);
            }
            R /= Kernel::Sum();
            G /= Kernel::Sum();
            B /= Kernel::Sum();
            return Color(R, G, B, Src->Pixel(2*x, 2*y).A);
        }

        inline Color ProcessMidlePart(int x, int y)
        {
            int R = 0;
            int G = 0;
            int B = 0;
            for (int m = -Kernel::HalfSize(); m <= Kernel::HalfSize(); m++)
            {
                const Color* src = &Src->Pixel(2*x, 2*y + m);
                R += int(src->R) * Kernel::Value(m);
                G += int(src->G) * Kernel::Value(m);
                B += int(src->B) * Kernel::Value(m);
            }
            R /= Kernel::Sum();
            G /= Kernel::Sum();
            B /= Kernel::Sum();
            return Color(R, G, B, Src->Pixel(2*x, 2*y).A);
        }

        inline Color ProcessDownEdge(int x, int y)
        {
            int R = 0;
            int G = 0;
            int B = 0;
            const int maxY = Src->Height() - 1;
            for (int m = -Kernel::HalfSize(); m <= Kernel::HalfSize(); m++)
            {
                const Color* src = &Src->Pixel(2*x, maxY - abs(maxY - (2*y + m)));
                R += int(src->R) * Kernel::Value(m);
                G += int(src->G) * Kernel::Value(m);
                B += int(src->B) * Kernel::Value(m);
            }
            R /= Kernel::Sum();
            G /= Kernel::Sum();
            B /= Kernel::Sum();
            return Color(R, G, B, Src->Pixel(2*x, 2*y).A);
        }
    };

    template<class Kernel>
    const Image ScaleDown(const Image& src)
    {
        Tools::Profiler profiler("ScaleDown");
        int w = src.Width();
        int h = src.Height();

        Image res(w / 2, h / 2, src.ColorSpace());

        {
            // horizontal filter
            Parallel::TaskList<HScalingTask<Kernel> > tasks;
            int step = res.Height() / tasks.Count();
            int i = 0;
            int pos = 0;
            if (step != 0)
            {
                for (; i < tasks.Count() - 1; i++)
                {
                    tasks[i].Set(src, res, pos, pos + step);
                    pos += step;
                }
                tasks[i].Set(src, res, pos, res.Height());
                tasks.SpawnAndSync();
            } else
            {
                tasks[0].Set(src, res, pos, res.Height());
                tasks[0].Run();
            }
        }
        {
            // vertical filter
            Parallel::TaskList<VScalingTask<Kernel> > tasks;
            int step = res.Width() / tasks.Count();
            int i = 0;
            int pos = 0;
            if (step != 0)
            {
                for (; i < tasks.Count() - 1; i++)
                {
                    tasks[i].Set(src, res, pos, pos + step);
                    pos += step;
                }
                tasks[i].Set(src, res, pos, res.Width());
                tasks.SpawnAndSync();
            } else
            {
                tasks[0].Set(src, res, pos, res.Width());
                tasks[0].Run();
            }
        }
        return res;
    }
}