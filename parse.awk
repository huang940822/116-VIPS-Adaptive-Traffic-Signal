BEGIN {
    int_biggest = 0
    int_smallest = 1011927840
    int_total = 0
    int_count = 0

    ext_biggest = 0
    ext_smallest = 1011927840
    ext_total = 0
    ext_count = 0
}

{
    if ($1 == "middleware_internal" ) 
    {
        value = $6
        if(value > 0)
        {
            int_total += value
            int_count += 1

            if ( value > int_biggest) {
                int_biggest = value
            }
            if ( value < int_smallest) {
                int_smallest = value
            }
        }
    }

    if ($1 == "middleware_external"  ) 
    {
        value = $6
        if(value > 0)
        {
            ext_total += value
            ext_count += 1

            if ( value > ext_biggest) {
                ext_biggest = value
            }
            if ( value < ext_smallest) {
                ext_smallest = value
            }
        }
    }
}

END {

    

    int_total /= 1000
    int_biggest /= 1000
    int_smallest /= 1000

    ext_total /= 1000
    ext_biggest /= 1000
    ext_smallest /= 1000

    print "middleware_internal"
    print "int Total: ", int_total
    print "int Count: ", int_count
    print "int Average: ", int_total/int_count
    print "int Biggest: ", int_biggest
    print "int Smallest: ", int_smallest

    print ""
    
    print "middleware_external"
    print "ext Total: ", ext_total
    print "ext Count: ", ext_count
    print "ext Average: ", ext_total/ext_count
    print "ext Biggest: ", ext_biggest
    print "ext Smallest: ", ext_smallest
}