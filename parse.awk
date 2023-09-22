BEGIN {
    int_biggest = 0
    int_smallest = 1011927840
    int_total = 0
    int_count = 0

    ext_biggest = 0
    ext_smallest = 1011927840
    ext_total = 0
    ext_count = 0

    trc12_biggest = 0
    trc12_smallest = 1011927840
    trc12_total = 0
    trc12_count = 0

    f1_biggest = 0
    f1_smallest = 1011927840
    f1_total = 0
    f1_count = 0

    f2_biggest = 0
    f2_smallest = 1011927840
    f2_total = 0
    f2_count = 0

    f3_biggest = 0
    f3_smallest = 1011927840
    f3_total = 0
    f3_count = 0

    f4_biggest = 0
    f4_smallest = 1011927840
    f4_total = 0
    f4_count = 0

    f5_biggest = 0
    f5_smallest = 1011927840
    f5_total = 0
    f5_count = 0

    f6_biggest = 0
    f6_smallest = 1011927840
    f6_total = 0
    f6_count = 0
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

    if ($1 == "inner_handle_heartbeat_from_app" ) 
    {
        value = $6
        if(value > 0)
        {
            f1_total += value
            f1_count += 1

            if ( value > f1_biggest) {
                f1_biggest = value
            }
            if ( value < f1_smallest) {
                f1_smallest = value
            }
        }
    }

    if ($1 == "inner_handle_request_by_api_id" ) 
    {
        value = $6
        if(value > 0)
        {
            f2_total += value
            f2_count += 1

            if ( value > f2_biggest) {
                f2_biggest = value
            }
            if ( value < f2_smallest) {
                f2_smallest = value
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
    
    f1_total /= 1000
    f1_biggest /= 1000
    f1_smallest /= 1000

    f2_total /= 1000
    f2_biggest /= 1000
    f2_smallest /= 1000

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

    print ""
    print "inner_handle_heartbeat_from_app"
    print " Total: ", f1_total
    print " Count: ", f1_count
    print " Average: ", f1_total/f1_count
    print " Biggest: ", f1_biggest
    print " Smallest: ", f1_smallest

    print ""
    print "inner_handle_request_by_api_id"
    print " Total: ", f2_total
    print " Count: ", f2_count
    print " Average: ", f2_total/f2_count
    print " Biggest: ", f2_biggest
    print " Smallest: ", f2_smallest
}