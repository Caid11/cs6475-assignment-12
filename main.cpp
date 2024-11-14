#include <algorithm>
#include <llvm/ADT/APInt.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/ConstantRange.h>

#include <cstdint>
#include <vector>

#include <stdio.h>

using namespace llvm;

std::vector<ConstantRange> getConstantRangesForBitwidth( unsigned bitwidth )
{
    assert(bitwidth <= 32);

    unsigned maxVal = 0;
    for( unsigned i = 0; i < bitwidth; i++ )
        maxVal |= (1 << i);

    std::vector<ConstantRange> possibleRanges;
    for( unsigned lo = 0; lo <= maxVal; lo++ ) {
        for( unsigned hi = lo + 1; hi <= maxVal; hi++ )
        {
            possibleRanges.push_back(ConstantRange(APInt(bitwidth, lo), APInt(bitwidth, hi)));
        }
    }

    return possibleRanges;
}

std::vector<APInt> getConcreteValuesForDomain( const ConstantRange& r )
{
    assert(r.getBitWidth() <= 32);

    std::vector<APInt> concreteValues;

    uint64_t lo = r.getUnsignedMin().getZExtValue();
    uint64_t hi = r.getUnsignedMax().getZExtValue();
    for( uint64_t x = lo; x <= hi; x++ )
        concreteValues.push_back(APInt(r.getBitWidth(), x));

    return concreteValues;
}

ConstantRange getAbstractValueForSet( unsigned bitwidth, const std::vector<APInt>& concreteValues )
{
    if( concreteValues.empty() )
        return ConstantRange::getEmpty(bitwidth);

    uint64_t lo = 0;
    uint64_t hi = 0;
    for( const APInt& x : concreteValues )
    {
        if( x.getZExtValue() < lo )
            lo = x.getZExtValue();
        if( x.getZExtValue() > hi )
            hi = x.getZExtValue();
    }

    // We add 1 to hi because ConstantRange has an exclusive upper bound.
    return ConstantRange(
            APInt(bitwidth, lo),
            APInt(bitwidth, hi + 1));
}

APInt extendAPInt( const APInt& x )
{
    return APInt(x.getBitWidth() + 1, x.getZExtValue());
}

ConstantRange extendRangeBitWidth( const ConstantRange& r )
{
    return ConstantRange(extendAPInt(r.getLower()), extendAPInt(r.getUpper()));
}

ConstantRange decomposedUaddSat( const ConstantRange& x, const ConstantRange& y)
{
    // Increase the bitwidth of the ranges so we can perform a non-wrapping add.
    ConstantRange xExt = extendRangeBitWidth(x);
    ConstantRange yExt = extendRangeBitWidth(y);
    
    // Add
    ConstantRange addRes = xExt.add(yExt);
    
    // Perform the "saturate" operation by clamping each bound to the maximum possible value.
    uint64_t maxVal = 0;
    for( unsigned i = 0; i < x.getBitWidth(); i++ )
        maxVal |= (1 << i);

    uint64_t clampedLower = std::min(addRes.getUnsignedMin().getZExtValue(), maxVal);
    uint64_t clampedUpper = std::min(addRes.getUnsignedMax().getZExtValue(), maxVal);

    APInt newLower(x.getBitWidth(), clampedLower);
    APInt newUpper(x.getBitWidth(), clampedUpper);
    if( newLower == newUpper )
        return ConstantRange(newLower);
    else
        return ConstantRange::getNonEmpty(newLower, newUpper);
}

int main(int argc, char** argv)
{
    llvm::LLVMContext context;

    std::vector<ConstantRange> rangesToTest = getConstantRangesForBitwidth(6);

    uint64_t total = 0;
    uint64_t numDecomposedBetter = 0;
    uint64_t numDecomposedWorse = 0;
    uint64_t numEqual = 0;
    uint64_t numIncomparable = 0;

    for( const ConstantRange& x : rangesToTest )
    {
        for( const ConstantRange& y : rangesToTest )
        {
            total++;

            ConstantRange llvmRes = x.uadd_sat(y);
            ConstantRange decomposedRes = decomposedUaddSat(x, y);

            // printf("x: [%llu, %llu), y: [%llu, %llu), llvm: [%llu, %llu), mine: [%llu, %llu)\n",
            //         x.getLower().getZExtValue(), x.getUpper().getZExtValue(),
            //         y.getLower().getZExtValue(), y.getUpper().getZExtValue(),
            //         llvmRes.getLower().getZExtValue(), llvmRes.getUpper().getZExtValue(),
            //         decomposedRes.getLower().getZExtValue(), decomposedRes.getUpper().getZExtValue());

            // Results are incomparable if there is no overlap between them.
            if( llvmRes.getUnsignedMax().getZExtValue() < decomposedRes.getUnsignedMin().getZExtValue() ||
                decomposedRes.getUnsignedMax().getZExtValue() < llvmRes.getUnsignedMin().getZExtValue())
            {
                // printf("incomparable\n");
                numIncomparable++;
                continue;
            }

            std::vector<APInt> concreteLlvmRes = getConcreteValuesForDomain( llvmRes );
            std::vector<APInt> concreteDecomposedRes = getConcreteValuesForDomain( decomposedRes );

            if( concreteLlvmRes.size() == concreteDecomposedRes.size() ) {
                // printf("equal\n");
                numEqual++;
            } else if( concreteLlvmRes.size() < concreteDecomposedRes.size() ) {
                // printf("decomposed worse\n");
                numDecomposedWorse++;
            } else if( concreteLlvmRes.size() > concreteDecomposedRes.size() ) {
                // printf("decomposed better\n");
                numDecomposedBetter++;
            }
        }
    }

    printf("Num abstract value pairs tested: %llu\n", total);
    printf("Num with equal result: %llu\n", numEqual);
    printf("Num decomposed better: %llu\n", numDecomposedBetter);
    printf("Num composite better: %llu\n", numDecomposedWorse);
    printf("Num incomparable results %llu\n", numIncomparable);
}
