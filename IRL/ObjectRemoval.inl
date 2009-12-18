#include "ObjectRemoval.h"
#include "GaussianPyramid.h"
#include "BidirectionalSimilarity.h"

namespace IRL
{
    template<class PixelType>
    const Image<PixelType> RemoveObject(const ImageWithMask<PixelType>& img, OperationCallback<PixelType>* callback)
    {
        const int Levels = int(log((float)Minimum<int>(img.Image.Width(), img.Image.Height())));

        // calculate Gaussian pyramid for source image and mask
        GaussianPyramid<PixelType> source(img.Image, Levels);
        GaussianPyramid<Alpha8> mask(img.Mask, Levels);

        BidirectionalSimilarity<PixelType, true> solver;

        int total = 2 * Levels + Levels * (Levels - 1) / 2;
        int progress = 0;

        // coarse to fine iteration
        for (int i = Levels - 1; i >= 0; i--)
        {
            solver.Reset();
            solver.Source = source.Levels[i];
            solver.SourceMask = mask.Levels[i];
            solver.NNFIterations = 4 + i * 2;
            solver.Alpha = 0.5;
            if (solver.Target.IsValid())
            {
                solver.Target = MixImages(solver.Source, ScaleUp(solver.Target), solver.SourceMask);
                solver.SourceToTarget = ClampField(ScaleUp(solver.SourceToTarget), solver.Target);
                solver.TargetToSource = ClampField(ScaleUp(solver.TargetToSource), solver.Source);
            } else
            {
                solver.Target = solver.Source; // use existing image
                solver.SourceToTarget = MakeRandomField(solver.Source, solver.Target);
                solver.TargetToSource = MakeRandomField(solver.Target, solver.Source);
            }
            for (int j = 0; j < 2 + i; j++)
            {
                solver.Iteration(true);

                progress++;
                if (!(i == 0 && j == 1 + i) && callback)
                    callback->IntermediateResult(solver.Target, progress, total);
            }
        }

        if (callback) callback->OperationEnded(solver.Target);
        return solver.Target; // final image
    }
}