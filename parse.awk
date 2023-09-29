BEGIN {
    label1_biggest = 0
    label1_smallest = 1011927840
    label1_total = 0
    label1_count = 0
    label1_total_sos_us = 0

    label2_biggest = 0
    label2_smallest = 1011927840
    label2_total = 0
    label2_count = 0
    label2_total_sos_us = 0

    label3_biggest = 0
    label3_smallest = 1011927840
    label3_total = 0
    label3_count = 0
    label3_total_sos_us = 0

    label4_biggest = 0
    label4_smallest = 1011927840
    label4_total = 0
    label4_count = 0
    label4_total_sos_us = 0

    label5_biggest = 0
    label5_smallest = 1011927840
    label5_total = 0
    label5_count = 0
    label5_total_sos_us = 0
}

{
    if ($1 == "middleware_internal" ) 
    {
        value = $6
        if(value > 0)
        {
            label1_total += value
            label1_count += 1
            label1_total_sos_us += (value/1000) * (value/1000)

            if ( value > label1_biggest) {
                label1_biggest = value
            }
            if ( value < label1_smallest) {
                label1_smallest = value
            }
        }
    }

    if ($1 == "middleware_external"  ) 
    {
        value = $6
        if(value > 0)
        {
            label2_total += value
            label2_count += 1
            label2_total_sos_us += (value/1000) * (value/1000)

            if ( value > label2_biggest) {
                label2_biggest = value
            }
            if ( value < label2_smallest) {
                label2_smallest = value
            }
        }
    }

    if ($1 == "inner_handle_request_by_api_id" ) 
    {
        value = $6
        if(value > 0)
        {
            label3_total += value
            label3_count += 1
            label3_total_sos_us += (value/1000) * (value/1000)

            if ( value > label3_biggest) {
                label3_biggest = value
            }
            if ( value < label3_smallest) {
                label3_smallest = value
            }
        }
    }

    
    if ($1 == "inner_handle_heartbeat_from_app" ) 
    {
        value = $6
        if(value > 0)
        {
            label4_total += value
            label4_count += 1
            label4_total_sos_us += (value/1000) * (value/1000)

            if ( value > label4_biggest) {
                label4_biggest = value
            }
            if ( value < label4_smallest) {
                label4_smallest = value
            }
        }
    }
}

END {
    label1_total /= 1000
    label1_avg_us = label1_total/label1_count
    label1_var_us = (label1_total_sos_us / label1_count) - (label1_avg_us * label1_avg_us)
    label1_sd_us = sqrt(label1_var_us)
    label1_biggest /= 1000
    label1_smallest /= 1000
    print ""
    print "middleware_internal"
    print "count: ", label1_count
    print "avg_us: ", label1_avg_us
    print "sd_us: ", label1_sd_us
    print "Biggest: ", label1_biggest
    print "Smallest: ", label1_smallest

    label2_total /= 1000
    label2_avg_us = label2_total/label2_count
    label2_var_us = (label2_total_sos_us / label2_count) - (label2_avg_us * label2_avg_us)
    label2_sd_us = sqrt(label2_var_us)
    label2_biggest /= 1000
    label2_smallest /= 1000
    print ""
    print "middleware_external"
    print "label2 Count: ", label2_count
    print "label2 label2_avg_us: ", label2_avg_us
    print "label2 label2_sd_us: ", label2_sd_us
    print "label2 Biggest: ", label2_biggest
    print "label2 Smallest: ", label2_smallest

    label3_total /= 1000
    label3_avg_us = label3_total/label3_count
    label3_var_us = (label3_total_sos_us / label3_count) - (label3_avg_us * label3_avg_us)
    label3_sd_us = sqrt(label3_var_us)
    label3_biggest /= 1000
    label3_smallest /= 1000
    print ""
    print "inner_handle_request_by_api_id"
    print "count: ", label3_count
    print "avg_us: ", label3_avg_us
    print "sd_us: ", label3_sd_us
    print "Biggest: ", label3_biggest
    print "Smallest: ", label3_smallest

    label4_total /= 1000
    label4_avg_us = label4_total/label4_count
    label4_var_us = (label4_total_sos_us / label4_count) - (label4_avg_us * label4_avg_us)
    label4_sd_us = sqrt(label4_var_us)
    label4_biggest /= 1000
    label4_smallest /= 1000
    print ""
    print "inner_handle_heartbeat_from_app"
    print "count: ", label4_count
    print "avg_us: ", label4_avg_us
    print "sd_us: ", label4_sd_us
    print "Biggest: ", label4_biggest
    print "Smallest: ", label4_smallest

    label5_total /= 1000
    label5_avg_us = label5_total/label5_count
    label5_var_us = (label5_total_sos_us / label5_count) - (label5_avg_us * label5_avg_us)
    label5_sd_us = sqrt(label5_var_us)
    label5_biggest /= 1000
    label5_smallest /= 1000
    print ""
    print "nothing"
    print "count: ", label5_count
    print "avg_us: ", label5_avg_us
    print "sd_us: ", label5_sd_us
    print "Biggest: ", label5_biggest
    print "Smallest: ", label5_smallest
}