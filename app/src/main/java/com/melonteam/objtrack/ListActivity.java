package com.melonteam.objtrack;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.ListView;

import java.util.ArrayList;
import java.util.List;

/**
 * Created by zhangpengyu on 2017/4/6.
 */

public class ListActivity extends Activity{
    public static final String[] Datas = new String[]{"摄像头追踪","视频追踪"};
    public static final Class[] Activitys = new Class[]{MainActivity.class,VideoActivity.class};

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
                Intent intent = new Intent(ListActivity.this,Activitys[position]);
                startActivity(intent);
            }
        });
        setContentView(listView);
    }


}
