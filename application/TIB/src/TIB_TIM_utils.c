#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "TIB_TIM_utils.h"
#include "TIB_utils.h"
#include "TIB_config.h"
#include "config.h"
#include "log.h"
#include "asn1helper_if.h"
#include "j2735_codec.h"
#include "j2735_msg.h"
LOG_USE_MODULE(TIB);
TravelerInformation *p_tim;

int tim_msg_init(TravelerInformation **pp_tim)
{
    Malloc(*pp_tim,sizeof(TravelerInformation),"TravelerInformation");
    TravelerInformation *p_tim = *pp_tim;
    p_tim->msgCnt = 1;
    time_t rawtime;
    time(&rawtime);
    struct tm result;
    struct tm *timeinfo = localtime_r(&rawtime, &result);

    p_tim->regional_option = FALSE;
    p_tim->regional.count = 1;
    Malloc(p_tim->regional.tab, sizeof(Reg_TravelerInformationList), "Reg_TravelerInformationList");
    p_tim->regional.tab->regionId = NoRegion;
    
    p_tim->dataFrames.count = TIB_config.TIM_table.size;
    if (p_tim->dataFrames.count == 0) {
        printf("No TIM entries found in TIM_table. Aborting TIM initialization.\n");
        LOG_MSG_ERROR("No TIM entries found in TIM_table. Aborting TIM initialization.\n");
        return -1;
    }
    // max_amount of dataframes allowed is 8
    else if (p_tim->dataFrames.count>8)
    {
        p_tim->dataFrames.count = 8;
    }

    LOG_MSG_INFO("tim dataframes count: %d\n", TIB_config.TIM_table.size);
    Malloc(p_tim->dataFrames.tab, sizeof(TravelerDataFrame)* p_tim->dataFrames.count, "TravelerDataFrame");
    for (int i = 0; i < p_tim->dataFrames.count; i++) {
        TravelerDataFrame *tdf = &(p_tim->dataFrames.tab[i]);
        TIM_config_sign_t *TIM_signs = &vector_at(TIB_config.TIM_table, i);
            
        int viewAngle_start = TIM_signs->viewAngle[0];
        int viewAngle_end = TIM_signs->viewAngle[1];
        int broadcast_start = TIM_signs->BroadcastDirection[0];
        int broadcast_end = TIM_signs->BroadcastDirection[1];
        int directionality = TIM_signs->directionality;
        tdf->frameType = TIM_signs->FrameType;

        LOG_MSG_INFO("TIM viewAngle start: %d\n", viewAngle_start);
        LOG_MSG_INFO("TIM viewAngle end: %d\n", viewAngle_end);
        LOG_MSG_INFO("TIM broadcast_start: %d\n", broadcast_start);
        LOG_MSG_INFO("TIM broadcast_end: %d\n", broadcast_end);
        TIM_Position_Node_t *TIM_loc = &vector_at(TIM_signs->TimPosition, 0);
        LOG_MSG_INFO("TIM sign lat: %lf",TIM_loc->lat);
        LOG_MSG_INFO("TIM sign lon: %lf",TIM_loc->lon);
        
        tdf->startYear_option = TRUE;
        tdf->startYear = timeinfo->tm_year;
        tdf->startTime = (((timeinfo->tm_yday * 24) + timeinfo->tm_hour) * 60) + timeinfo->tm_min;
        tdf->msgId.choice = TDFmsgId_roadSignID;    
        tdf->msgId.u.roadSignID.position.lat = TIM_loc->lat * 10000000;
        tdf->msgId.u.roadSignID.position.Long = TIM_loc->lon * 10000000;
        tdf->msgId.u.roadSignID.position.elevation = 0;
        tdf->msgId.u.roadSignID.position.regional_option = TRUE;
        tdf->msgId.u.roadSignID.position.regional.count = 1;
        Malloc(tdf->msgId.u.roadSignID.position.regional.tab, sizeof(Reg_Position3D),"Reg_Position3D");
        tdf->msgId.u.roadSignID.position.regional.tab[0].regionId = TIM_Adjust_RegionalID;
        
        tdf->priority = 4;
        Malloc(tdf->msgId.u.roadSignID.viewAngle.buf, 16, "TIM view angle");    
        tdf->msgId.u.roadSignID.viewAngle.len = 16;
        if (0<=viewAngle_start<16 && 0<=viewAngle_end<16)
        {
            if (viewAngle_start>viewAngle_end)
            {
                for (int a=0;a<=viewAngle_end;a++)
                {
                    asn1_bstr_set_bit(&(tdf->msgId.u.roadSignID.viewAngle),a);                
                }
                for (int a=viewAngle_start;a<16;a++)
                {
                    asn1_bstr_set_bit(&(tdf->msgId.u.roadSignID.viewAngle),a);                
                }
            }
            else if (viewAngle_start<viewAngle_end)
            {
                for (int b=viewAngle_start;b<=viewAngle_end;b++)
                {
                    asn1_bstr_set_bit(&(tdf->msgId.u.roadSignID.viewAngle),b);                
                }
            }
            else
            {
                for (int i=0;i<16;i++)
                {
                    asn1_bstr_set_bit(&(tdf->msgId.u.roadSignID.viewAngle),i);
                }
            }
        }
        else
        {
            LOG_MSG_INFO("broadcast ViewAngle is illegal\n");
        }
        

        tdf->regions.count=1;
        Malloc(tdf->regions.tab,sizeof(GeographicalPath)*tdf->regions.count,"GeographicalPath");
        GeographicalPath *geo = &(tdf->regions.tab[0]);
        geo->id_option = TRUE;
        geo->id.region_option = TRUE;
        geo->id.region = config.RSU_region;
        geo->anchor_option = TRUE;
        geo->anchor.Long = TIM_signs->anchor[0] * 10000000;
        geo->anchor.lat = TIM_signs->anchor[1] * 10000000;
        geo->laneWidth = 100;
        if (0<=directionality<=3)
        {
            geo->directionality = directionality;
        }
        else
        {
            printf("illegal directionality\n");
            geo->directionality = 0;
        }
        geo->closedPath = FALSE;
        Malloc(geo->direction.buf, 16, "TIM direction BitString status");
        geo->direction_option = TRUE;
        geo->direction.len = 16;
        if (0<=broadcast_start<16 && 0<=broadcast_end<16)
        {
            if (broadcast_start>broadcast_end)
            {
                for (int a=0;a<=broadcast_end;a++)
                {
                    asn1_bstr_set_bit(&(geo->direction),a);
                }
                for (int a=broadcast_start;a<16;a++)
                {
                    asn1_bstr_set_bit(&(geo->direction),a);
                }
            }
            else if (broadcast_start<broadcast_end)
            {
                for (int b=broadcast_start;b<=broadcast_end;b++)
                {
                    asn1_bstr_set_bit(&(geo->direction),b);
                }
            }
            else
            {
                for (int i=0;i<16;i++)
                {
                    asn1_bstr_set_bit(&(geo->direction),i);
                }
            }
        }
        
        
        geo->description_option = TRUE;
        geo->description.choice = GeographicalPathDescription_path;
        geo->description.u.path.scale_option = FALSE;
        geo->description.u.path.offset.choice = OffsetValue_xy;
        // initialize geo in node-LatLon form
        NodeListXY *viewpath = &geo->description.u.path.offset.u.xy;
        
        int node_count = TIM_signs->TimPath.size;
        viewpath->choice = NodeListXY_nodes;
        viewpath->u.nodes.count = node_count;
        
        viewpath->u.nodes.tab = (NodeXY *)calloc(node_count, sizeof(NodeXY));
        for (int j=0;j<node_count;j++)
        {
            TIM_Path_Node_t *node_pos = &vector_at(TIM_signs->TimPath, j);
            printf("updating pos : %lf %lf\n",node_pos->lat, node_pos->lon);
            LOG_MSG_INFO("updating pos : %lf %lf\n",node_pos->lat, node_pos->lon);
            viewpath->u.nodes.tab[j].delta.choice = NodeOffsetPointXY_node_LatLon;
            viewpath->u.nodes.tab[j].delta.u.node_LatLon.lat = (node_pos->lat) * 10000000;
            viewpath->u.nodes.tab[j].delta.u.node_LatLon.lon = (node_pos->lon) * 10000000;
            
            printf("updated viewpath node %d lat: %d ", j, viewpath->u.nodes.tab[j].delta.u.node_LatLon.lat);
            printf("lon: %d \n", viewpath->u.nodes.tab[j].delta.u.node_LatLon.lon);
            LOG_MSG_INFO("updated viewpath node %d lat: %d lon: %d \n", j, viewpath->u.nodes.tab[j].delta.u.node_LatLon.lat, viewpath->u.nodes.tab[j].delta.u.node_LatLon.lon);
        }
        
        geo->regional_option = FALSE;

        tdf->content.choice = TIM_signs->eventType;
        printf("TIM sign eventType: %d\n",TIM_signs->eventType);
        int content_count = sizeof(TIM_signs->EventDescription) / sizeof(TIM_signs->EventDescription[0]);
        if (TIM_signs->eventType==TDFcontent_workZone) {
            printf("TIM workZone: ");
            tdf->content.u.workZone.count = content_count;
            Malloc(tdf->content.u.workZone.tab,(sizeof(WorkZone))*tdf->content.u.workZone.count,"genericSign");
            for (int i=0; i<tdf->content.u.workZone.count; i++) {
                printf("%d ",TIM_signs->EventDescription[i]);
                tdf->content.u.workZone.tab[i].choice = ITISitem_itis;
                tdf->content.u.workZone.tab[i].u.itis = TIM_signs->EventDescription[i];
            }
            printf("\n");
        }
        else if (TIM_signs->eventType==TDFcontent_genericSign) {
            printf("TIM genericSign: ");
            tdf->content.u.genericSign.count = content_count;
            Malloc(tdf->content.u.genericSign.tab,(sizeof(GenericSignage))*tdf->content.u.genericSign.count,"genericSign");
            for (int i=0; i<tdf->content.u.genericSign.count; i++) {
                printf("%d ",TIM_signs->EventDescription[i]);
                tdf->content.u.genericSign.tab[i].choice = ITISitem_itis;
                tdf->content.u.genericSign.tab[i].u.itis = TIM_signs->EventDescription[i];
            }
            printf("\n");
        }
        else if (TIM_signs->eventType==TDFcontent_speedLimit) {
            printf("TIM speedlimit: ");
            tdf->content.u.speedLimit.count = content_count;
            Malloc(tdf->content.u.speedLimit.tab,(sizeof(SpeedLimit))*tdf->content.u.speedLimit.count,"speedLimit");
            for (int i=0; i<tdf->content.u.speedLimit.count; i++) {
                printf("%d ",TIM_signs->EventDescription[i]);
                tdf->content.u.speedLimit.tab[i].choice = ITISitem_itis;
                tdf->content.u.speedLimit.tab[i].u.itis = TIM_signs->EventDescription[i];
            }
            printf("\n");
        }
        
        else if (TIM_signs->eventType==TDFcontent_exitService) {
            printf("TIM exitService: ");
            tdf->content.u.exitService.count = content_count;
            Malloc(tdf->content.u.exitService.tab,(sizeof(ExitService))*tdf->content.u.exitService.count,"exitService");
            for (int i=0; i<tdf->content.u.exitService.count; i++) {
                tdf->content.u.exitService.tab[i].choice = ITISitem_itis;
                tdf->content.u.exitService.tab[i].u.itis = TIM_signs->EventDescription[i];
            }
            printf("\n");
        }
    }    
    return 0;
}

int tim_msg_update(TravelerInformation *pp_tim) //用config決定內容，update更新時間
{
    time_t rawtime;
    time(&rawtime);
    struct tm result;
    struct tm *timeinfo = localtime_r(&rawtime, &result);

    for (int i = 0; i < pp_tim->dataFrames.count; i++) {
        TravelerDataFrame tdf = pp_tim->dataFrames.tab[i];
        pp_tim->timeStamp = (((timeinfo->tm_yday * 24) + timeinfo->tm_hour) * 60) + timeinfo->tm_min;
        tdf.startYear = timeinfo->tm_year;
        tdf.startTime = (((timeinfo->tm_yday * 24) + timeinfo->tm_hour) * 60) + timeinfo->tm_min;
    }
    
    return 1;
}

void tim_printf(TravelerInformation *pp_tim)
{
    printf("amount of TIM dataframes: %d\n",pp_tim->dataFrames.count);
    for (int i = 0; i < pp_tim->dataFrames.count; i++) {
        TIM_config_sign_t *TIM_signs = &vector_at(TIB_config.TIM_table, i);
        TravelerDataFrame *tdf = &pp_tim->dataFrames.tab[i];
        TravelerDataFrame *tdf_test_print = pp_tim->dataFrames.tab;
        printf("nowtime : %d %d\n", tdf->startYear, pp_tim->timeStamp);
        printf("priority : %d\n", tdf->priority);
        printf("viewAngle heading : ");
        char buf[17];
        memset(buf,0,17);
        int bits;
        for (int a=0; a<16; a++) {
            bits = asn1_bstr_is_bit_set(&(tdf->msgId.u.roadSignID.viewAngle),a);
            printf("%d",bits);
            snprintf(buf + strlen(buf), 17 - strlen(buf), "%d", bits);
        }
        printf("\n");
        LOG_MSG_INFO("viewAngle heading : %s\n", buf);
        memset(buf,0,17);

        //GeographicalPath *geo = tdf.regions.tab;
        GeographicalPath *geo = &(tdf->regions.tab[0]);
        GeographicalPath *geo_test_print = tdf_test_print->regions.tab;
        int anchor_lat = geo->anchor.lat;
        LOG_MSG_INFO("geo location: %f %f\n", tdf->regions.tab->anchor.Long/10000000.0, anchor_lat/10000000.0);
        LOG_MSG_INFO("geo region: %d\n", geo_test_print->id.region);
        
        printf("TIM broadcast direction: ");
        for (int a=0; a<16; a++) {
            bits = asn1_bstr_is_bit_set(&(geo->direction),a);
            printf("%d",bits);
            //LOG_MSG_INFO("%d",bits);
            snprintf(buf + strlen(buf), 16 - strlen(buf), "%d", bits);
        }
        printf("\n");
        LOG_MSG_INFO("TIM broadcast direction: %s\n", buf);
        //NodeSetXY nodes = tdf.regions.tab->description.u.path.offset.u.xy.u.nodes;
        NodeListXY *viewpath = &geo->description.u.path.offset.u.xy;
        NodeSetXY *nodes = &geo->description.u.path.offset.u.xy.u.nodes;
        NodeSetXY *nodes_test = &geo_test_print->description.u.path.offset.u.xy.u.nodes;
        
        //printf("nodes count: %d\n",tdf.regions.tab->description.u.path.offset.u.xy.u.nodes.count);
        printf("nodes count: %d\n",viewpath->u.nodes.count);
        LOG_MSG_INFO("viewpath nodes count: %d\n", viewpath->u.nodes.count);
        
        if (viewpath->u.nodes.count!=0)
        {
            for (int a=0; a<viewpath->u.nodes.count; a++) 
            {
                printf("TIM path node %d description choice: %d\n", a, viewpath->u.nodes.tab[a].delta.choice);
                printf("TIM path node %d pos: %d %d \n", a, viewpath->u.nodes.tab[a].delta.u.node_LatLon.lon, 
                    viewpath->u.nodes.tab[a].delta.u.node_LatLon.lat);
                LOG_MSG_INFO("TIM path node %d pos: %lf %lf \n", a, viewpath->u.nodes.tab[a].delta.u.node_LatLon.lon/10000000.0, 
                    viewpath->u.nodes.tab[a].delta.u.node_LatLon.lat/10000000.0);
                
            }
        }
        
        LOG_MSG_INFO("TIM content type: %d\n",tdf->content.choice);
        if (tdf->content.choice==TDFcontent_genericSign) {
            int count = tdf->content.u.genericSign.count;
            for (int i=0;i<count;i++) {
                if (tdf->content.u.genericSign.tab[i].u.itis!=0)
                {
                    LOG_MSG_INFO("TIM Sign ITIS code %d: %d\n",i,tdf->content.u.genericSign.tab[i].u.itis);
                }
            }
        }
        else if (tdf->content.choice==TDFcontent_workZone) {
            int count = tdf->content.u.workZone.count;
            for (int i=0;i<count;i++) {
                if (tdf->content.u.genericSign.tab[i].u.itis!=0)
                {
                    LOG_MSG_INFO("TIM workZone ITIS code %d: %d\n",i,tdf->content.u.workZone.tab[i].u.itis);
                }                
            }
        }
        else if (tdf->content.choice==TDFcontent_speedLimit) {
            int count = tdf->content.u.speedLimit.count;
            for (int i=0;i<count;i++) {
                if (tdf->content.u.speedLimit.tab[i].u.itis!=0)
                {
                    LOG_MSG_INFO("TIM speedLimit ITIS code %d: %d\n",i,tdf->content.u.speedLimit.tab[i].u.itis);
                }                
            }
        }
    }
    
}