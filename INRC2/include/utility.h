//
// -------+--------+------
//       low      high
//
// if target is less than low, the distance is (low - target),
// if target is greater than high, the distance is (target - high)
// else, the distance is 0
// 
// require operator-() and operator<()
template <typename T>
T distanceToRange( const T &target, const T &lowerBound, const T &upperBound )
{
    T dist = lowerBound - target;
    if (dist < 0) {
        dist = target - upperBound;
        if (dist < 0) {
            dist = 0;
        }
    }
    return dist;
}

//
// ---------+-----------
//        bound
//
// if target is greater than high, the distance is (target - high)
// else, the distance is 0
// 
// require operator-() and operator<()
template <typename T>
T exceedBound( const T &target, const T &bound )
{
    T diff( target - bound );
    if (0 < diff) {
        return diff;
    }
}