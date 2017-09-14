package com.example.qianwu.shopify2018winter;

import android.app.DownloadManager;
import android.content.Intent;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.IOException;

import okhttp3.OkHttpClient;
import okhttp3.Callback;
import okhttp3.OkHttpClient;
import okhttp3.Request;
import okhttp3.Response;

import org.json.JSONObject;

public class MainActivity extends AppCompatActivity {

    private TextView resultOfMoneySpent;
    private TextView resultOfItemSold;
    private Button calculateButton;
    private int itemSold;
    private String uri = "https://shopicruit.myshopify.com/admin/orders.json?page=1&access_token=c32313df0d0ef512ca64d5b336a0d7c6"; // this uri is set for the shopify api
    private double totalInCAD = 0;

    private JSONObject mJSONObject;


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        resultOfMoneySpent = (TextView)findViewById(R.id.moneySpent);
        resultOfItemSold = (TextView)findViewById(R.id.itemSold);
        calculateButton = (Button)findViewById(R.id.button);

        request();
        calculateButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                calculateRevenue();
                calculateNumberOfProduct();

            }
        });

    }




    public JSONObject request(){

        OkHttpClient client = new OkHttpClient();



        Request request = new Request.Builder().url(uri).build();

        okhttp3.Call call = client.newCall(request);
        call.enqueue(new Callback() {
            @Override
            public void onFailure(okhttp3.Call call, IOException e) {
                Log.d("FAIL","TRUE");

            }

            @Override
            public void onResponse(okhttp3.Call call, Response response) throws IOException {

                try {
                    String jsonData = response.body().string();
                    //Response response = call.execute();
                    if (response.isSuccessful()) {

                        try{
                            Log.d("jsonData is",jsonData);
                            mJSONObject = new JSONObject(jsonData);

                        }

                        catch (Exception e){
                            Log.d("exception caught","1");
                        }


                    }
                    else{
                    }
                }
                catch (IOException e) {

                    Log.d("exception caught","2");
                }

            }
        });
        return mJSONObject;
    }


    public void calculateRevenue(){// using the uri to calculate the result;

        JSONArray array = null;
        try{
            array = mJSONObject.getJSONArray("orders");
        }
        catch (Exception r){
            Log.d("array","not retrieved");
        }

        totalInCAD = 0;
        try {
            for(int i = 0; i< array.length();++i){
                if(array.getJSONObject(i).getString("email").equals("napoleon.batz@gmail.com")){
                    totalInCAD += Double.parseDouble(array.getJSONObject(i).getString("total_line_items_price"));
                    Log.d("napoleon","1");
                }
            }
        }
        catch (Exception r){
            Log.d("not displaying","2");
        }
        resultOfMoneySpent.setText(totalInCAD+" CAD");

    }


    public void calculateNumberOfProduct(){ // using the uri to calculate the number

        itemSold = 0;
        JSONArray array = null;
        try{
            array = mJSONObject.getJSONArray("orders");
        }
        catch (Exception r){
            Log.d("array","not retrieved");
        }
        try {
            for(int i = 0; i< array.length();++i){
                for(int ii = 0; ii<array.getJSONObject(i).getJSONArray("line_items").length(); ++ii){




                    if((array.getJSONObject(i)
                            .getJSONArray("line_items")
                            .getJSONObject(ii).getString("title")).equals("Awesome Bronze Bag")){

                        ++itemSold;
                        Log.d(itemSold+"","1");

                    }

                    Log.d("current size of arr",array.getJSONObject(i).getJSONArray("line_items").length()+"");
                }
            }
        }
        catch (Exception r){
            Log.d("not displaying","21");
        }


        resultOfItemSold.setText(itemSold+" sold");
    }


}
