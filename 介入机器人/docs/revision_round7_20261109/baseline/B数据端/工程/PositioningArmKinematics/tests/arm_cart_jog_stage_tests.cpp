#include "arm_cart_jog_stage_reference.h"
#include "arm_cart_jog_stage_generated.h"
#include "arm_cart_jog_stage_before_generated.h"
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>

using Bytes=std::vector<unsigned char>;
template<class T>void append(Bytes& out,const T& x){const auto* p=reinterpret_cast<const unsigned char*>(&x);out.insert(out.end(),p,p+sizeof(x));}
template<class M>void sample(Bytes& out,const M& m){
    append(out,m.stage);append(out,m.G.arm_cjog_owned);append(out,m.G.arm_cjog_state);append(out,m.G.arm_cjog_error);
    append(out,m.alive_age);append(out,m.enable_age);append(out,m.zero_cycles);append(out,m.fault_stop);
    append(out,m.gen.path_speed);append(out,m.gen.target_speed);append(out,m.gen.limited);
    for(int i=1;i<=5;++i){append(out,m.gen.p[i]);append(out,m.gen.v[i]);append(out,m.gen.a[i]);
        append(out,m.dir_feed[i]);append(out,m.feed_count[i]);for(double value:m.last_feed[i])append(out,value);
        for(const auto* fb:{&m.fb_enable[i],&m.fb_disable[i],&m.fb_stop[i]}){
            append(out,fb->execute);append(out,fb->Done);append(out,fb->Error);append(out,fb->Busy);
            append(out,fb->Enabled);append(out,fb->ErrorID);append(out,fb->position);append(out,fb->jerk);
        }
    }
}
template<class M>void normal_script(M& m,int n){
    m.first_cycle=n==0;
    m.G.arm_cjog_mode=n<=2500?1:0;m.G.arm_cjog_dir=n<=2500?1:0;m.G.arm_cjog_speed=n<=2500?5:0;
    ++m.G.arm_cjog_alive;
    for(int i=1;i<=5;++i){m.fb_enable[i].script={};m.fb_enable[i].script.Enabled=m.stage==10||m.stage==25;
        m.fb_disable[i].script={};m.fb_disable[i].script.Done=m.stage==40;
        m.fb_stop[i].script={};m.fb_stop[i].script.Done=m.stage==50;}
}
template<class M>Bytes normal_trace(){M m;Bytes data;for(int n=0;n<3200;++n){normal_script(m,n);m.step();sample(data,m);}return data;}

int main(int argc,char**argv){
    if(argc<2||argc>3)return 2;
    const bool capture=argc==3&&std::strcmp(argv[2],"--capture")==0;
    if(argc==3&&!capture)return 2;
    const Bytes trace=normal_trace<StCartJogStage>();
    if(capture){const auto before=normal_trace<StCartJogStageBefore>();
        std::ofstream file(argv[1],std::ios::binary);file.write(reinterpret_cast<const char*>(before.data()),before.size());
        std::cout<<"CAPTURE pre-round4 normal stage trace cycles=3200 bytes="<<before.size()<<'\n';return file?0:1;}
    std::ifstream file(argv[1],std::ios::binary);const Bytes baseline((std::istreambuf_iterator<char>(file)),{});
    const Bytes reference=normal_trace<JogStageReference>();
    const Bytes before=normal_trace<StCartJogStageBefore>();
    for(const auto& item:{std::pair<const char*,const Bytes*>{"pre-round4",&baseline},{"reference",&reference}}){
        const auto mismatch=std::mismatch(trace.begin(),trace.end(),item.second->begin(),item.second->end());
        std::cout<<"S5.3 compare="<<item.first<<" bytes="<<trace.size()<<" other_bytes="<<item.second->size()
            <<" first_difference="<<std::distance(trace.begin(),mismatch.first)<<'\n';
    }
    int failures=0;std::uint64_t cycles=3200;
    bool normal_discrete_equal=true;
    JogStageReference normal_ref;StCartJogStage normal_st;
    for(int n=0;n<3200;++n){normal_script(normal_ref,n);normal_script(normal_st,n);normal_ref.step();normal_st.step();
        normal_discrete_equal=normal_discrete_equal&&normal_ref.request_valid==normal_st.request_valid
            &&normal_ref.held==normal_st.held&&normal_ref.drive_ready==normal_st.drive_ready
            &&normal_ref.last_alive==normal_st.last_alive&&normal_ref.all_enabled==normal_st.all_enabled
            &&normal_ref.all_disabled==normal_st.all_disabled&&normal_ref.all_stopped==normal_st.all_stopped
            &&normal_ref.all_released==normal_st.all_released
            &&normal_ref.gen.core.branch==normal_st.gen.core.branch
            &&normal_ref.gen.core.latched_mode==normal_st.gen.core.latched_mode
            &&normal_ref.gen.core.latched_dir==normal_st.gen.core.latched_dir;}

    const auto result=[&](bool ok,const char* name){std::cout<<(ok?"PASS ":"FAIL ")<<name<<'\n';if(!ok)++failures;};
    result(trace==baseline&&trace==reference&&trace==before&&normal_discrete_equal,"S5.3 normal release: 3200 cycles identical to pre-round4 ST and handwritten reference");
    static_assert(StCartJogStage::ALIVE_TIMEOUT==JogStageReference::ALIVE_TIMEOUT);
    static_assert(StCartJogStage::ENABLE_TIMEOUT==JogStageReference::ENABLE_TIMEOUT);
    for(int scenario:{1,2}){
        JogStageReference ref;StCartJogStage st;
        ref.stage=st.stage=scenario==1?25:40;
        ref.G.arm_cjog_owned=st.G.arm_cjog_owned=true;
        const std::uint32_t root_error=scenario==1?3002:0x1234;
        int released=0;bool equal=true,skip_error_axis=true,healthy_stop=false,root_kept=true,waited_for_busy=false;
        for(int n=1;n<=15;++n){
            if(scenario==1)ref.G.arm_axis[2].Status.Error=true;
            ++ref.G.arm_cjog_alive;st.G=ref.G;
            for(int i=1;i<=5;++i){
                FbFrame enable,disable,stop;
                disable.Done=n>=2;
                if(scenario==2&&i==3){disable.Done=false;disable.Error=true;disable.ErrorID=0x1234;}
                stop.Busy=n>=3&&n<=6;
                stop.Done=n>=5;
                if(scenario==1&&i==2){stop.Error=n>=2;stop.ErrorID=0x4650;stop.Busy=false;stop.Done=false;}
                if(scenario==2&&i==1&&n>=3){stop.Error=true;stop.ErrorID=0x5678;stop.Busy=false;stop.Done=false;}
                ref.fb_enable[i].script=st.fb_enable[i].script=enable;
                ref.fb_disable[i].script=st.fb_disable[i].script=disable;
                ref.fb_stop[i].script=st.fb_stop[i].script=stop;
            }
            ref.step();st.step();++cycles;
            Bytes a,b;sample(a,ref);sample(b,st);equal=equal&&a==b
                &&ref.all_enabled==st.all_enabled&&ref.all_disabled==st.all_disabled
                &&ref.all_stopped==st.all_stopped&&ref.all_released==st.all_released;
            if(scenario==1){skip_error_axis=skip_error_axis&&!st.fb_stop[2].execute;
                healthy_stop=healthy_stop||st.fb_stop[1].execute;}
            if(n==6)waited_for_busy=st.stage==60&&st.G.arm_cjog_owned;
            if(st.G.arm_cjog_error!=0)root_kept=root_kept&&st.G.arm_cjog_error==root_error;
            if(st.stage==0&&!st.G.arm_cjog_owned&&released==0)released=n;
        }
        std::cout<<"S5."<<scenario<<" release_cycle="<<released<<" cycles_after_other_axes_done="<<released-5
            <<" error="<<st.G.arm_cjog_error<<" error_axis_stop_execute="<<!skip_error_axis<<'\n';
        result(equal&&released>=5&&released<=15&&root_kept&&waited_for_busy&&st.G.arm_cjog_error==root_error
            &&(scenario!=1||(skip_error_axis&&healthy_stop)),scenario==1?
            "S5.1 axis 2 error, Stop error and Busy release":"S5.2 Disable error releases and preserves first error");
    }
    // 错误码在空闲和坏请求期间保持，只有下一次成功起步才清零。
    JogStageReference ref;StCartJogStage st;ref.G.arm_cjog_error=0x4650;st.G=ref.G;
    bool invalid_keeps_first=true;
    for(int mode:{9,1}){ref.G.arm_cjog_mode=mode;ref.G.arm_cjog_dir=1;ref.G.arm_cjog_speed=5;
        ++ref.G.arm_cjog_alive;st.G=ref.G;ref.step();st.step();++cycles;
        if(mode==9)invalid_keeps_first=ref.G.arm_cjog_error==0x4650&&st.G.arm_cjog_error==0x4650;}
    result(invalid_keeps_first&&ref.G.arm_cjog_error==0&&st.G.arm_cjog_error==0&&ref.stage==10&&st.stage==10,"first error clears on successful start");
    std::cout<<"S5 ST versus reference cycles="<<cycles<<" bitwise_exact="<<(failures==0)<<" max_numerical_difference="<<(failures==0?"0":"see failed comparison")<<" failures="<<failures<<'\n';
    return failures?1:0;
}
