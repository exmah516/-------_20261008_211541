#include "st_runtime.h"
#include "Old.h"
#include "New.h"
#include "Common.h"
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <set>

static int checks = 0, cycles = 0;
static void check(bool ok, const char* label)
{
    ++checks;
    if (!ok) { std::cerr << "FAIL " << label << '\n'; std::exit(1); }
}
static double difference(const Node& a, const Node& b, const std::string& path = "")
{
    double result = std::abs(a.value-b.value);
    for (const auto& entry : a.fields)
    {
        const std::string next = path+"."+entry.first;
        if ((entry.first.find("handoff_")==0 || entry.first=="cyl_cmd" || entry.first=="return_cmd" || entry.first=="axis1_fast_return" || entry.first=="axis6_fast_retract" || entry.first=="startup_smoothing_bypass") || entry.first == "err_backoff_active" || entry.first == "err_backoff_failed" ||
            entry.first == "axis2_sync_error" || entry.first == "axis2_sync_error_id" ||
            entry.first == "rslimit_distance" || entry.first.find("recovery_") == 0 ||
            entry.first.find("homing_restore_") == 0 || entry.first.find("axis15_") == 0 ||
            entry.first.find("err_backoff_") == 0 || entry.first == "return_clear_error_id") continue;
        const auto found = b.fields.find(entry.first);
        const double delta = difference(entry.second, found == b.fields.end() ? Node{} : found->second, next);
        if (delta > 0) { std::cerr << "DIFF " << next << " " << delta << '\n'; return delta; }
        result = std::max(result,delta);
    }
    for (const auto& entry : b.fields)
        if (a.fields.count(entry.first)==0 && entry.second.value != 0 &&
            entry.first.find("handoff_") != 0 && entry.first!="cyl_cmd" && entry.first!="return_cmd" && entry.first!="axis1_fast_return" && entry.first!="axis6_fast_retract" && entry.first!="startup_smoothing_bypass" && entry.first.find("recovery_") != 0 && entry.first.find("err_backoff_") != 0 &&
            entry.first.find("axis15_") != 0 && entry.first.find("homing_restore_") != 0 &&
            entry.first != "return_clear_error_id") return std::abs(entry.second.value);
    return result;
}
static void healthy(Node& G)
{
    G["self_check_done"] = true; G["host_comm_timeout"] = false;
    G["host_session_id"] = 1; G["host_heartbeat_sequence"] = 1; G["gen_state"] = _handle;
    for (int axis=1;axis<=7;++axis)
    {
        G["power_output"][axis]["Done"] = true;
        G["SetPointGenEnable"][axis]["Done"] = true;
        G["SetPointGenDisable"][axis]["Done"] = true;
        G["axis"][axis]["NcToPlc"]["ActPos"] = 20+axis;
    }
}
template<class Old, class New> static void compare(Old& old, New& now, Node& a, Node& b, const char* label)
{
    old.cycle(); now.cycle(); ++cycles;
    const double delta=difference(a,b);
    check(delta==0,label);
    const auto locals=commonLocals(old,now);
    check(difference(locals.first,locals.second)==0,"common internal state identical");
}
template<class SelfCheck> static void selfcheck_feedback(SelfCheck& sc, Node& g)
{
    for(int axis=1;axis<=7;++axis)
    {
        auto& nc=g["axis"][axis]["NcToPlc"];
        auto& status=g["axis"][axis]["Status"];
        auto& velocity=sc.fb_move_vel[axis];
        auto& absolute=sc.fb_move_abs[axis];
        auto& stop=sc.fb_stop[axis];
        nc["ActVelo"]=0;
        velocity["Busy"]=double(velocity["Execute"]);
        sc.fb_set_pos[axis]["Done"]=double(sc.fb_set_pos[axis]["Execute"]);
        stop["Done"]=double(stop["Execute"]);
        if(velocity["Execute"] && !stop["Execute"])
        {
            const double limit=-5.0-axis;
            nc["ActPos"]=std::max(limit,double(nc["ActPos"])-0.001*double(velocity["Velocity"]));
            status["DriveLimitActive"]=nc["ActPos"]<=limit;
            nc["ActVelo"]=status["DriveLimitActive"]?0:-double(velocity["Velocity"]);
        }
        absolute["Busy"]=false; absolute["Done"]=false;
        if(absolute["Execute"] && !stop["Execute"])
        {
            const double delta=double(absolute["Position"])-nc["ActPos"];
            const double step=0.001*double(absolute["Velocity"]);
            const double moved=std::max(-step,std::min(step,delta));
            nc["ActPos"]=double(nc["ActPos"])+moved;
            nc["ActVelo"]=moved/0.001;
            absolute["Done"]=std::abs(delta)<=step;
            absolute["Busy"]=!absolute["Done"];
            if(std::abs(moved)>0) status["DriveLimitActive"]=false;
        }
    }
}
int main()
{
    std::ofstream scenes("normal_scenarios.csv");
    scenes<<"scenario,cycles,max_difference,completion\n";
    Node a,b; initOld(a); initNew(b); healthy(a); healthy(b);
    Oldhandle old(a); Newhandle now(b);
    for(int n=0;n<1000;++n)
    {
        st_tick=n; a["host_heartbeat_sequence"]=b["host_heartbeat_sequence"]=n+1;
        for(int axis=1;axis<=7;++axis)
            a["refer"][axis]=b["refer"][axis]=std::sin(n*0.01)*0.2;
        compare(old,now,a,b,"normal handle outputs identical");
    }
    check(old.init_done && now.init_done && b["handle_reinit_done"],"normal follow includes completed initialization");
    scenes<<"handle_follow,1000,0,initialized_and_following\n";
    Node ea,eb; initOld(ea); initNew(eb); healthy(ea); healthy(eb);
    Olderr olderr(ea); Newerr newerr(eb);
    for(int n=0;n<2500;++n)
    {
        st_tick=n;
        ea["axis"][1]["Status"]["Error"]=eb["axis"][1]["Status"]["Error"]=n<230;
        olderr.enter=newerr.enter=n==0;
        compare(olderr,newerr,ea,eb,"ordinary err outputs identical");
    }
    check(ea["gen_state"]==_clear_err && eb["gen_state"]==_clear_err,"ordinary err reaches clear_err");
    scenes<<"ordinary_err,2500,0,clear_err\n";
    Oldclear_err oc(ea); Newclear_err nc(eb);
    for(int n=0;n<250;++n) { st_tick=2500+n; compare(oc,nc,ea,eb,"clear_err outputs identical"); }
    check(eb["gen_state"]==_handle && eb["handle_reinit_req"],"clear_err completes settle and requests handle reinit");
    scenes<<"clear_err,250,0,handle_reinit_requested\n";
    Node sa,sb; initOld(sa); initNew(sb); healthy(sa); healthy(sb);
    OldSelfCheck os(sa); NewSelfCheck ns(sb);
    for(int axis=1;axis<=7;++axis) os.stu[axis]=ns.stu[axis]=10;
    for(int n=0;n<10;++n) { st_tick=3000+n; compare(os,ns,sa,sb,"normal SelfCheck completion outputs identical"); }
    scenes<<"selfcheck_finish_block,10,0,self_check_done\n";
    {
        Node left,right; initOld(left); initNew(right); healthy(left); healthy(right);
        left["self_check_done"]=right["self_check_done"]=false;
        left["gen_state"]=right["gen_state"]=_self_check;
        left["selfcheck_start_req"]=right["selfcheck_start_req"]=true;
        left["selfcheck_reset_req"]=right["selfcheck_reset_req"]=true;
        for(int axis=1;axis<=7;++axis)
            left["selfcheck_target_from_left"][axis]=right["selfcheck_target_from_left"][axis]=20+axis*2;
        OldSelfCheck before(left); NewSelfCheck after(right); OldMAIN dispatcher_before(left); NewMAIN dispatcher_after(right);
        dispatcher_before.self_check_.hook=[&] {before.init_target_from_left=dispatcher_before.self_check_["init_target_from_left"];before.cycle();};
        dispatcher_after.self_check_.hook=[&] {after.init_target_from_left=dispatcher_after.self_check_["init_target_from_left"];after.cycle();};
        std::set<int> visited[8];
        std::ofstream trace("selfcheck_full_trace.csv");
        trace<<"cycle,state,done,stu1,stu2,stu3,stu4,stu5,stu6,stu7,max_difference\n";
        trace<<"-1,"<<double(right["gen_state"])<<",0";
        for(int axis=1;axis<=7;++axis) trace<<','<<double(after.stu[axis]);
        trace<<",0\n";
        int count=0;
        for(;count<15000 && !right["self_check_done"];++count)
        {
            st_tick=10000+count;
            left["host_heartbeat_sequence"]=right["host_heartbeat_sequence"]=count+1;
            selfcheck_feedback(before,left); selfcheck_feedback(after,right);
            for(int axis=1;axis<=7;++axis) visited[axis].insert(int(after.stu[axis]));
            compare(dispatcher_before,dispatcher_after,left,right,"SelfCheck start-to-done MAIN outputs identical");
            const auto locals=commonLocals(before,after);
            check(difference(locals.first,locals.second)==0,"SelfCheck start-to-done local states identical");
            trace<<count<<','<<double(right["gen_state"])<<','<<double(right["self_check_done"]);
            for(int axis=1;axis<=7;++axis) {visited[axis].insert(int(after.stu[axis]));trace<<','<<double(after.stu[axis]);}
            trace<<",0\n";
        }
        check(left["self_check_done"] && right["self_check_done"] && right["gen_state"]==_handle,"full SelfCheck starts at zero and reaches done/handle");
        for(int axis:{1,3,5,6}) for(int stage:{0,1,2,3,6,7,71,72,8,10})
            check(visited[axis].count(stage)!=0,"full SelfCheck visits each normal translational phase");
        for(int axis:{2,7}) for(int stage:{0,1,2,10}) check(visited[axis].count(stage)!=0,"full SelfCheck visits rotary homing phases");
        check(visited[4].count(0) && visited[4].count(10),"full SelfCheck keeps axis4 through disable and complete");
        std::cout<<"PASS SelfCheck start-to-self_check_done cycles="<<count<<" max_difference=0\n";
        scenes<<"selfcheck_start_to_done,"<<count<<",0,self_check_done_true_and_handle\n";
    }
    Node ma,mb; initOld(ma); initNew(mb); healthy(ma); healthy(mb);
    ma["gen_state"]=mb["gen_state"]=_self_check;
    OldMAIN om(ma); NewMAIN nm(mb);
    for(int n=0;n<20;++n) { st_tick=4000+n; compare(om,nm,ma,mb,"MAIN normal dispatch outputs identical"); }
    scenes<<"MAIN_idle_dispatch,20,0,selfcheck_wait\n";
    Node aa,ab; initOld(aa); initNew(ab); healthy(aa); healthy(ab);
    OldAxis2Sync oa(aa); NewAxis2Sync na(ab);
    aa["power_"][2]["Enable"]=ab["power_"][2]["Enable"]=true;
    oa.fb_power["Status"]=na.fb_power["Status"]=true;
    for(int n=0;n<1000;++n)
    {
        st_tick=5000+n;
        oa.fb_gear["InGear"]=na.fb_gear["InGear"]=n>2;
        compare(oa,na,aa,ab,"normal Axis2Sync outputs identical");
    }
    scenes<<"Axis2Sync,1000,0,in_gear\n";
    {
        Node left,right;initOld(left);initNew(right);healthy(left);healthy(right);
        left["gen_state"]=right["gen_state"]=_err;
        OldMAIN before(left);NewMAIN after(right);Olderr err_before(left);Newerr err_after(right);
        Oldclear_err clear_before(left);Newclear_err clear_after(right);Oldhandle handle_before(left);Newhandle handle_after(right);
        before.err_.hook=[&]{err_before.enter=double(before.err_["enter"]);err_before.cycle();};
        after.err_.hook=[&]{err_after.enter=double(after.err_["enter"]);err_after.cycle();};
        before.clear_err_.hook=[&]{clear_before.cycle();};after.clear_err_.hook=[&]{clear_after.cycle();};
        before.handle1_.hook=[&]{handle_before.cycle();};after.handle1_.hook=[&]{handle_after.cycle();};
        std::set<int> states;
        int count=0;
        for(;count<5000;++count)
        {
            st_tick=20000+count;
            left["host_heartbeat_sequence"]=right["host_heartbeat_sequence"]=count+1;
            left["axis"][1]["Status"]["Error"]=right["axis"][1]["Status"]["Error"]=count<230;
            states.insert(int(right["gen_state"]));
            compare(before,after,left,right,"ordinary err MAIN-to-handle outputs identical");
            for(const auto& local:{commonLocals(err_before,err_after),commonLocals(clear_before,clear_after),commonLocals(handle_before,handle_after)})
                check(difference(local.first,local.second)==0,"ordinary err full-chain internal state identical");
            if(right["gen_state"]==_handle && right["handle_reinit_done"] && handle_after.init_done && !right["estop_hold_req"]) break;
        }
        check(count<5000 && states.count(_err) && states.count(_clear_err) && states.count(_handle),"ordinary err completes clear_err and handle reinitialization");
        scenes<<"ordinary_err_MAIN_to_handle,"<<count+1<<",0,handle_reinit_done_and_hold_released\n";
        std::cout<<"PASS ordinary err full-chain cycles="<<count+1<<" max_difference=0\n";
    }
    const int exit_checks_start = checks;
    // 新出口：每个测试直接驱动新ST的真实局部状态和FB输出。
    {
        Node g; initNew(g); healthy(g); Newhandle h(g);
        g["SetPointGenEnable"][1]["Done"]=false;
        g["SetPointGenEnable"][1]["Error"]=true;
        st_tick=0; h.cycle(); check(!g["SetPointGenEnable"][1]["Execute"], "P05 error drops Execute");
        ++st_tick; h.cycle(); check(g["SetPointGenEnable"][1]["Execute"], "P05 one full low cycle then retrigger");
        g["SetPointGenEnable"][1]["Error"]=false; g["SetPointGenEnable"][1]["Done"]=true;
        ++st_tick; h.cycle(); check(h.init_done, "P05 retry succeeds");
    }
    {
        Node g; initNew(g); healthy(g); Newhandle h(g);
        g["SetPointGenEnable"][1]["Done"]=false;
        for(int n=0;n<=2001;++n) { st_tick=n; h.cycle(); }
        check(g["gen_state"]==_err, "P05 2s timeout enters err");
        Node e; initNew(e); healthy(e); Newhandle hi(e); e["axis"][1]["Status"]["Error"]=true;
        hi.cycle(); check(e["gen_state"]==_err, "P05 initialization detects axis error before return");
    }
    {
        Node g; initNew(g); healthy(g); Newhandle h(g); h.init_done=true; h.return_state[1]=10;
        h.handoff["leg_cmd"][1]["Req"]=true; g["SetPointGenDisable"][1]["Enabled"]=true; g["SetPointGenDisable"][1]["Done"]=false;
        for(int n=0;n<=2001;++n) { st_tick=n; h.cycle(); }
        check(h.handoff["leg_cmd"][1]["Error"] || h.return_finish_error[1], "P06 disable timeout finishes with error");
        Node e; initNew(e); healthy(e); Newhandle he(e); he.init_done=true; he.return_state[1]=10;
        he.handoff["leg_cmd"][1]["Req"]=true; e["SetPointGenDisable"][1]["Error"]=true; e["SetPointGenDisable"][1]["ErrorID"]=123;
        he.cycle(); he.cycle(); check(he.handoff["leg_cmd"][1]["ErrorId"]==123, "P06 disable ErrorID propagated");
    }
    {
        Node g; initNew(g); healthy(g); Newhandle h(g); h.init_done=true; h.return_state[1]=20;
        h.handoff["abort_motion"]=true; g["axis"][1]["NcToPlc"]["ActVelo"]=1;
        h.cycle(); check(h.return_clear_stop[1]["Execute"] && h.return_finish_error[1], "P07 Clear stops running motion");
        check(h.handoff["leg_error"][1]==3, "P07 executor abort classification");
        g["axis"][1]["NcToPlc"]["ActVelo"]=0; h.return_clear_stop[1]["Done"]=true;
        h.cycle(); h.cycle(); check(!h.return_clear_stop[1]["Execute"], "P07 Stop released before Enable");
        h.cycle(); check(h.handoff["leg_cmd"][1]["Error"] && !h.handoff["leg_cmd"][1]["Busy"], "P07 Clear completes with PLC error and Busy false");
    }
    {
        Node g; initNew(g); healthy(g); Newhandle h(g); h.init_done=true; h.return_state[1]=31;
        g["SetPointGenEnable"][1]["Error"]=true; g["SetPointGenEnable"][1]["Done"]=false;
        for(int n=0;n<=2001;++n) { st_tick=n; h.cycle(); }
        check(h.return_enable_retry[1]==3 && g["gen_state"]==_err, "P07 Enable retries three then 2s err");
    }
    for(int state:{10,20})
    {
        Node g; initNew(g); healthy(g); Newhandle h(g); h.init_done=true; h.return_state[1]=state;
        h.handoff["leg_cmd"][1]["Req"]=true; g["SetPointGenDisable"][1]["Enabled"]=true;
        g["SetPointGenDisable"][1]["Done"]=false;
        g["axis"][1]["NcToPlc"]["ActVelo"]=1;
        for(int n=0;n<=2000;++n){st_tick=n;h.cycle();}
        check(h.return_finish_error[1] && h.return_state[1]==30, "P06/P07 each stage2s timeout finishes error");
        if(state==20) check(h.return_clear_stop[1]["Execute"],"P07 execution timeout stops movement");
    }
    {
        Node g; initNew(g); healthy(g); Newhandle h(g); h.init_done=true; h.return_state[1]=31;
        g["SetPointGenEnable"][1]["Error"]=true;g["SetPointGenEnable"][1]["Done"]=false;
        st_tick=0;h.cycle();check(!g["SetPointGenEnable"][1]["Execute"],"P07 Enable drops on Error");
        ++st_tick;h.cycle();check(g["SetPointGenEnable"][1]["Execute"],"P07 Enable reissues after low cycle");
        g["SetPointGenEnable"][1]["Error"]=false;g["SetPointGenEnable"][1]["Done"]=true;
        ++st_tick;h.cycle();check(h.return_state[1]==0,"P07 Enable succeeds after retry");
    }
    for(int state:{10,30,40})
    {
        Node g;initNew(g);healthy(g);Newhandle h(g);h.init_done=true;h.axis4_manual_state=state;
        g["axis4_fwd_req"]=true;g["axis"][4]["NcToPlc"]["ActVelo"]=1;
        Node* fb=state==10?&g["SetPointGenDisable"][4]:state==30?&h.fb_axis4_stop:&g["SetPointGenEnable"][4];
        (*fb)["Error"]=true;(*fb)["Done"]=false;
        st_tick=0;h.cycle();++st_tick;h.cycle();
        check((*fb)["Execute"],"P10 Error retriggers after low cycle");
        (*fb)["Error"]=false;(*fb)["Done"]=true;++st_tick;h.cycle();
        check(h.axis4_manual_state!=state,"P10 retry success exits each wait");
    }
    for(int state : {10,30,40})
    {
        Node g; initNew(g); healthy(g); Newhandle h(g); h.init_done=true; h.axis4_manual_state=state;
        g["axis4_fwd_req"]=true; g["axis"][4]["NcToPlc"]["ActVelo"]=1;
        if(state==10) { g["SetPointGenDisable"][4]["Error"]=true; g["SetPointGenDisable"][4]["Done"]=false; }
        if(state==30) h.fb_axis4_stop["Error"]=true;
        if(state==40) { g["SetPointGenEnable"][4]["Error"]=true; g["SetPointGenEnable"][4]["Done"]=false; }
        for(int n=0;n<10;++n) { st_tick=n; h.cycle(); }
        check(h.axis4_manual_state==40 || g["axis4_manual_error"] || g["gen_state"]==_err, "P10 each Error exits wait after three retriggers");
        Node t; initNew(t); healthy(t); Newhandle ht(t); ht.init_done=true; ht.axis4_manual_state=state;
        t["axis4_fwd_req"]=true; t["axis"][4]["NcToPlc"]["ActVelo"]=1;
        t["SetPointGenDisable"][4]["Done"]=false; t["SetPointGenDisable"][4]["Enabled"]=true;
        t["SetPointGenEnable"][4]["Done"]=false;
        for(int n=0;n<=2001;++n) { st_tick=n; ht.cycle(); }
        check(ht.axis4_manual_state==40 || t["gen_state"]==_err, "P10 each 2s timeout exits wait");
    }
    for(bool right : {false,true})
    {
        Node g; initNew(g); healthy(g); Newerr e(g); e.step=1;
        g["axis"][1]["Status"]["DriveLimitActive"]=true;
        g["axis"][1]["NcToPlc"]["ActPos"]=right?99:0;
        e.cycle(); check(e.step==20 && e.backoff_distance[1]==(right?-2:2), "R6 nearest limit chooses opposite direction");
        e.backoff_move[1]["Done"]=true; g["axis"][1]["Status"]["DriveLimitActive"]=false;
        e.cycle(); e.cycle(); e.cycle(); check(g["gen_state"]==_clear_err, "R6 success returns to clear_err");
        Node f; initNew(f); healthy(f); Newerr ef(f); ef.step=1; f["axis"][1]["Status"]["DriveLimitActive"]=true;
        ef.backoff_move[1]["Error"]=true;
        for(int n=0;n<20;++n) ef.cycle();
        check(ef.backoff_attempts[1]==3 && f["err_backoff_failed"][1] && f["gen_state"]!=_clear_err, "R6 exactly three failures then original err loop");
        Node u; initNew(u); healthy(u); u["self_check_done"]=false; u["axis"][1]["Status"]["DriveLimitActive"]=true;
        Newerr eu(u); eu.step=1; eu.cycle(); check(eu.backoff_attempts[1]==0 && u["gen_state"]==_clear_err, "R6 no homing skips backoff");
    }
    {
        Node g; initNew(g); healthy(g); Newhandle h(g); h.init_done=true;
        g["axis2_sync_axis"]["Status"]["Error"]=true; h.cycle();
        check(g["gen_state"]==_handle, "R7 Axis15 alone does not pause body");
        NewAxis2Sync sync(g); st_tick=0; sync.cycle(); check(sync.fb_reset["Execute"], "R7 starts independent reset pulse");
        st_tick=200; sync.cycle(); check(!sync.fb_reset["Execute"], "R7 pulse ends at200ms");
        for(int tick=201;tick<=2000;++tick) { st_tick=tick; sync.cycle(); }
        check(sync.fb_reset["Execute"], "R7 repeats at2s");
        g["axis2_sync_axis"]["Status"]["Error"]=false; sync.fb_power["Status"]=true; g["power_"][2]["Enable"]=true;
        ++st_tick; sync.cycle(); check(sync.gear_execute, "R7 resynchronizes after reset");
    }
    {
        Node reference; initNew(reference); healthy(reference); NewSelfCheck sc(reference);
        for(int axis=1;axis<=7;++axis) sc.stu[axis]=10;
        sc.cycle();
        Node g; initNew(g); healthy(g); NewMAIN main(g); g["gen_state"]=_self_check;
        g["homing_restore_req"]=true; g["homing_restore_leftlimit"]=reference["leftlimit"]; g["homing_restore_init_pos"]=reference["init_pos"];
        for(int axis=1;axis<=7;++axis) { g["SetPointGenDisable"][axis]["Enabled"]=true; g["SetPointGenDisable"][axis]["Done"]=false; }
        main.cycle(); check(g["homing_restore_status"]==0, "R9 waits for external setpoint disable");
        for(int axis=1;axis<=7;++axis) g["SetPointGenDisable"][axis]["Done"]=true;
        main.cycle(); check(g["homing_restore_status"]==1 && !g["startup_loading_ready"], "R9 applied without loading posture flag");
        for(const char* field : {"leftlimit","init_pos","cylinder1_value","cylinder2_value","cylinder3_value","cylinder4_value",
            "self_check_done","handle_reinit_req","Act_pos","act_pos_from_left","refer","ref_slow","act_h","act_hf","refer_from_left","gen_state"})
            check(difference(reference[field],g[field])==0, "R9 finish variables equal SelfCheck finish");
        g["gen_state"]=_self_check; g["homing_restore_req"]=true; g["power_output"][1]["Done"]=false;
        main.cycle(); check(g["homing_restore_status"]==2 && g["homing_restore_reject_reason"]==1, "R9 PLC rejects not-ready axes");
    }
    std::cout << "PASS V8/V9/V10/V11 new ST exits; checks=" << checks-exit_checks_start << "\n";
    std::cout << "PASS baseline/current complete ST: cycles=" << cycles << " max_difference=0\n";
    std::cout << "PASS checks=" << checks << '\n';
    // 固定参考阶跃的条件计算，不把缺失的手柄物理位移上界当作已知值。
    std::ofstream calculation("catchup.csv");
    calculation << "axis,target_step,unit,settle_ms,peak_velocity,peak_acceleration\n";
    for(int axis=1;axis<=7;++axis) for(int example=0;example<(axis==1||axis==6?2:1);++example)
    {
        const bool linear=axis==1||axis==3||axis==5||axis==6;
        const double displacement=example?(axis==1?25.0:32.0):(linear?7.5:5.729577951308232);
        Node g; initNew(g); healthy(g);
        for(int i=1;i<=7;++i) g["axis"][i]["NcToPlc"]["ActPos"]=0;
        Newhandle h(g);
        for(int n=0;n<40;++n) { st_tick=n;g["host_heartbeat_sequence"]=n+1;h.cycle(); }
        double peak_velocity=0,peak_acceleration=0;
        int settled=-1;
        for(int n=1;n<=5000;++n)
        {
            st_tick=40+n;g["host_heartbeat_sequence"]=n+41;g["refer"][axis]=displacement;
            h.cycle();
            peak_velocity=std::max(peak_velocity,std::abs(double(h.traj_vel[axis])));
            peak_acceleration=std::max(peak_acceleration,std::abs(double(h.traj_acc[axis])));
            if(std::abs(double(h.traj_pos[axis])-displacement)<0.001 && std::abs(double(h.traj_vel[axis]))<0.01)
            { settled=n;break; }
        }
        if(settled<0) { std::cerr<<"FAIL catchup calculation did not settle\n";return 1; }
        calculation<<axis<<','<<displacement<<','<<(linear?"mm":"deg")<<','<<settled<<','<<peak_velocity<<','<<peak_acceleration<<'\n';
    }
}
