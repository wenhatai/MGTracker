package com.melonteam.objtrack;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.ListView;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

/**
 * Created by zhangpengyu on 2017/4/6.
 */

public class ListActivity extends Activity{
    public static final String[] Datas = new String[]{"摄像头追踪","镜头移动前景固定","背景固定前景移动",
            "视频追踪-Basketball","视频追踪-DragonBady","视频追踪-MotorRolling","视频追踪-Bolt2"};
    public static final Class[] Activitys = new Class[]{MainActivity.class,VideoActivity.class};

    public static final String[] FileNames = new String[]{"jjcmmv.mp4","jjbkfix.mp4",
            "basketball.mp4","DragonBady.mp4", "MotorRolling.mp4","Bolt2.mp4" };

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        ListView listView = new ListView(this);
        List<String> dataList = new ArrayList<>();
        for(int i = 0;i< Datas.length;i++){
            dataList.add(Datas[i]);
        }
        listView.setAdapter(new ArrayAdapter<String>(this, android.R.layout.simple_expandable_list_item_1,dataList));
        listView.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                Intent intent = new Intent(ListActivity.this,Activitys[position > 0 ? 1: 0]);
                if(position > 0){
                    File outFile = new File(getExternalFilesDir(null), FileNames[position-1]);
                    intent.putExtra("FilePath",outFile.getAbsolutePath());
                }
                startActivity(intent);
            }
        });
        setContentView(listView);
    }


}
