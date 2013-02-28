/*++

Module Name:

    SmarterPairedEndAligner.h

Abstract:

    A more sophisticated paired-end aligner.

Authors:

    Matei Zaharia, February, 2012

Environment:

    User mode service.

Revision History:

--*/

#pragma once

#include "PairedEndAligner.h"
#include "BaseAligner.h"
#include "FixedSizeMap.h"
#include "FixedSizeVector.h"
#include "BigAlloc.h"
#include "directions.h"
#include "IntersectingPairedEndAligner.h"


class SmarterPairedEndAligner : public PairedEndAligner
{
public:
    SmarterPairedEndAligner(
        GenomeIndex  *index_,
        unsigned      maxReadSize_,
        unsigned      confDiff_,
        unsigned      maxHits_,
        unsigned      maxK_,
        unsigned      maxSeeds_,
        unsigned      minSpacing_,                 // Minimum distance to allow between the two ends.
        unsigned      maxSpacing_,                 // Maximum distance to allow between the two ends.
        unsigned      adaptiveConfDiffThreshold_); // Increase confDiff if this many seeds in the read have multiple hits.
    
    virtual ~SmarterPairedEndAligner();
    
    virtual void align(
        Read                  *read0,
        Read                  *read1,
        PairedAlignmentResult *result);

    void *operator new(size_t size) {return BigAlloc(size);}
    void operator delete(void *ptr) {BigDealloc(ptr);}

    static size_t getBigAllocatorReservation(unsigned maxHitsToConsider, unsigned maxReadSize, unsigned seedLen, unsigned maxSeedsToUse);
    void *operator new(size_t size, BigAllocator *allocator) {_ASSERT(size == sizeof(SmarterPairedEndAligner)); return allocator->allocate(size);}
    void operator delete(void *ptr, BigAllocator *allocator) {/* do nothing, the owner of the allocator is responsible for cleaning up the memory */}

private:
    static const int BUCKET_SIZE = 16;
    static const int INFINITE_SCORE = 0x7FFF;
    static const int MAX_READ_SIZE = 10000;
    static const int MAX_SEED_SIZE = 32;
    static const int NUM_READS_PER_PAIR = 2;    // This is just to make it clear what the array subscripts are, it doesn't ever make sense to change
    
    char complement[256];
    int wrapOffset[MAX_SEED_SIZE];
    
    GenomeIndex *index;
    int seedLen;
    
    unsigned maxReadSize;
    
    unsigned confDiff;
    unsigned maxHits;
    unsigned maxK;
    unsigned maxSeeds;
    unsigned minSpacing;
    unsigned maxSpacing;
    unsigned adaptiveConfDiffThreshold;
    int maxBuckets;
    
    BaseAligner *singleAligner;
    BaseAligner *mateAligner;
    IntersectingPairedEndAligner *intersectingAligner;

    CountingBigAllocator countingAllocator; // BJB - for the intersecting aligner for now.

    BoundedStringDistance <> *boundedStringDist;
    LandauVishkin<1> lv;
    LandauVishkin<-1> reverseLV;

    struct Bucket {
        unsigned found;                  // Bit vector for sub-locations matched
        unsigned scored;                 // Bit vector for sub-locations scored
        unsigned score;                  // Best score for any element in the bucket
        double matchProbability;         // Match probability of the element represented by score
        unsigned short bestOffset;       // Offset that gave us the best score (if any)
        unsigned short seedHits;         // Number of seeds that hit this bucket
        unsigned short disjointSeedHits; // Number of disjoint seeds that hit this bucket
        unsigned short minPairScore;     // Lower bound on the bucket's pair score (if not known)
        AlignmentResult mateStatus;      // If we've searched for a mate nearby, this is the result
        int mateScore;                   // Score of the mate found nearby, if any
        unsigned mateLocation;           // Location of the mate found nearby, if any
        double mateProbability;          // match probability for the mate
        
        inline bool allScored() { return scored == found; }

        void *operator new[](size_t size) {return BigAlloc(size);}
        void operator delete[](void *ptr) {BigDealloc(ptr);}
    };

    struct Candidate {
        char read;
        Direction direction;
        short seedHits;
        unsigned bucketLoc;
        Bucket *bucket;

        Candidate(char read_, Direction direction_, unsigned bucketLoc_, Bucket *bucket_, short seedHits_)
            : read(read_), direction(direction_), bucketLoc(bucketLoc_), bucket(bucket_), seedHits(seedHits_) {}

        Candidate() {}
    };

    Bucket *buckets;
    int bucketsUsed;
    
    FixedSizeMap<unsigned, Bucket*> bucketTable[2][NUM_DIRECTIONS];     // [read][Direction]
    FixedSizeVector<unsigned> bucketLocations[2][NUM_DIRECTIONS];       // [read][Direction]
    
    FixedSizeVector<Candidate> candidates;
    
    void alignTogether(Read *reads[NUM_READS_PER_PAIR], PairedAlignmentResult *result, int lowerBound[NUM_READS_PER_PAIR]);

    void clearState();
    
    Bucket *newBucket();
    
    Bucket *getBucket(int read, Direction direction, unsigned location);
    
    void computeRC(Read *read, char *outputBuf);
    
    void scoreBucket(Bucket *bucket, int readId, Direction direction, unsigned location, int seedOffset, int seedLength,
                     const char *readData, const char *reversedReaData, const char *qualityString, const char *reversedQualityString,
                     int readLen, int scoreLimit, double *matchProbability);

    void scoreBucketMate(Bucket *bucket, int readId, Direction direction, unsigned location, Read *mate, int scoreLimit, int *mateMapq);
    
    // Absolute difference between two unsigned values.
    inline unsigned distance(unsigned a, unsigned b) { return (a > b) ? a - b : b - a; }
    
    // Get the confDiff value to use given # of popular seeds in each [read][Direction] orientation.
    int getConfDiff(int seedsTried, int popularSeeds[NUM_READS_PER_PAIR][NUM_DIRECTIONS], int seedHits[2][NUM_DIRECTIONS]);

    // Sort candidates in decreasing order of seed hits.
    static inline bool compareCandidates(const Candidate &c1, const Candidate &c2) {
        return c1.seedHits > c2.seedHits;
    }

    char *reversedRead[NUM_READS_PER_PAIR][NUM_DIRECTIONS];
    char *reversedQuality[NUM_READS_PER_PAIR];


};
