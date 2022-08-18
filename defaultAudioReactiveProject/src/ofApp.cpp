/*
 * Copyright (c) 2013 Dan Wilcox <danomatika@gmail.com>
 *
 * BSD Simplified License.
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution.
 *
 * See https://github.com/danomatika/ofxMidi for documentation
 *
 */
#include "ofApp.h"
#include "iostream"
#define MIDI_MAGIC 63.50f
#define CONTROL_THRESHOLD .025f

//dummy variables for midi to audio attenuatiors
//0 is direct midi, 1 is low_x, 2 is mid_x, 3 is high_x
int control_switch=0;

//put these into arrays
const int controlSize=17;
float fftLow[controlSize];
float fftMid[controlSize];
float fftHigh[controlSize];

float low_value_smoothed=0.0;
float mid_value_smoothed=0.0;
float high_value_smoothed=0.0;
float smoothing_rate=.8;

float fft_low=0;
float fft_mid=0;
float fft_high=0;
//the range changes depending on what yr using for sound input.  just usb cams have shitter ranges
	//so 50 works
float range=200;

bool clear_switch=0;


//p_lock biz
//maximum total size of the plock array
const int p_lock_size=240;
bool p_lock_record_switch=0;
bool p_lock_erase=0;
//maximum number of p_locks available...maybe there can be one for every knob
//for whatever wacky reason the last member of this array of arrays has a glitch
//so i guess just make an extra array and forget about it for now
const int p_lock_number=17;
//so how we will organize the p_locks is in multidimensional arrays
//to access the data at timestep x for p_lock 2 (remember counting from 0) we use p_lock[2][x]
float p_lock[p_lock_number][p_lock_size];
//smoothing parameters(i think for all of these we can array both the arrays and the floats
//for now let us just try 1 smoothing parameter for everything.
float p_lock_smooth=.5;
//and then lets try an array of floats for storing the smoothed values
float p_lock_smoothed[p_lock_number];
//turn on and off writing to the array
bool p_lock_0_switch=1;
//global counter for all the locks
int p_lock_increment=0;

float my_normalize=0;



//--------------------------------------------------------------
void ofApp::setup() {
	//ofSetVerticalSync(true);
	ofSetFrameRate(30);
    ofBackground(0);
	//ofToggleFullscreen();
	ofHideCursor();
	
    //shader1.load("shadersES2/shader1");
	midiSetup();
	
	//fft biz
	fft.setup();
    fft.setNormalize(false);
    //fft.setVolumeRange(1.0f);
	fftArrayClear();
	p_lockClear();
}
//-----------------------------------------------------------
void ofApp::fftArrayClear(){
	for(int i=0;i<controlSize;i++){
		fftLow[i]=0;
		fftMid[i]=0;
		fftHigh[i]=0;
	}
	
}
//--------------------------------------------------------------
void ofApp::update() {
	//this one points to the ofxFft library
	fft.update();
	//this one is local and made by me
	fftValuesUpdate();
	
	midibiz();
	p_lockUpdate();
}
//-------------------------------------------------------------
void ofApp::fftValuesUpdate(){
	//so we can use this to remap things regularly, or like just if you hit a switch to renormalize
	if(fft.getLowVal()>my_normalize){
		my_normalize=fft.getLowVal();
	 //cout<<"highest possible"<<my_normalize<<endl;
	 }
	
	fft_low=fft.getLowVal();
	fft_low=ofClamp(fft_low,1.0f,range);
	fft_low=fft_low/range;
	low_value_smoothed=fft_low*smoothing_rate+(1.0f-smoothing_rate)*low_value_smoothed;
	
	fft_mid=fft.getMidVal();
	fft_mid=ofClamp(fft_mid,1.0f,range);
	fft_mid=fft_mid/range;
	mid_value_smoothed=fft_mid*smoothing_rate+(1.0f-smoothing_rate)*mid_value_smoothed;
	
	fft_high=fft.getHighVal();
	fft_high=ofClamp(fft_high,1.0f,range);
	fft_high=fft_high/range;
	high_value_smoothed=fft_high*smoothing_rate+(1.0f-smoothing_rate)*high_value_smoothed;
	
}
//--------------------------------------------------------------
void ofApp::draw() {
	
	
	
	
	ofSetColor(255);
	//ofDrawBitmapString("fps =" + ofToString(ofGetFrameRate())+"fft low =" + ofToString(fft_low)+"fft low_smoothed =" + ofToString(low_value_smoothed)+"fft mid =" + ofToString(fft_low)+"fft mid_smoothed =" + ofToString(mid_value_smoothed), 10, ofGetHeight() - 5 );
	ofDrawBitmapString("fft high =" + ofToString(fft_high,2)+
	"fft high_smoothed =" + ofToString(high_value_smoothed,2)+
	"fft mid =" + ofToString(fft_mid,2)+
	"fft mid_smoothed =" + ofToString(mid_value_smoothed,2)+
	"fft low =" + ofToString(fft_low,2)+
	"fft low_smoothed =" + ofToString(low_value_smoothed,2)
	, 10, ofGetHeight() - 5 );
}
//--------------------------------------------------------------
void ofApp::exit() {
	// clean up
	midiIn.closePort();
	midiIn.removeListener(this);
}

//-------------------------------------------------------------
void ofApp::p_lockUpdate(){
	for(int i=0;i<p_lock_number;i++){
        p_lock_smoothed[i]=p_lock[i][p_lock_increment]*(1.0f-p_lock_smooth)+p_lock_smoothed[i]*p_lock_smooth;
        if(abs(p_lock_smoothed[i])<.01){p_lock_smoothed[i]=0;}
    }
	if(p_lock_record_switch==1){
        p_lock_increment++;
        p_lock_increment=p_lock_increment%p_lock_size;
    }
}

//------------------------------------------------------
void ofApp::p_lockClear(){
	for(int i=0;i<p_lock_number;i++){
        for(int j=0;j<p_lock_size;j++){
            p_lock[i][j]=0; 
        }//endplocksize
    }//endplocknumber
}

//--------------------------------------------------------------
void ofApp::newMidiMessage(ofxMidiMessage& msg) {

	// add the latest message to the message queue
	midiMessages.push_back(msg);

	// remove any old messages if we have too many
	while(midiMessages.size() > maxMessages) {
		midiMessages.erase(midiMessages.begin());
	}
}

//----------------------------------------------------------
void ofApp::midiSetup(){
	// print input ports to console
	midiIn.listInPorts();
	
	// open port by number (you may need to change this)
	midiIn.openPort(1);
	//midiIn.openPort("IAC Pure Data In");	// by name
	//midiIn.openVirtualPort("ofxMidiIn Input"); // open a virtual port
	
	// don't ignore sysex, timing, & active sense messages,
	// these are ignored by default
	midiIn.ignoreTypes(false, false, false);
	
	// add ofApp as a listener
	midiIn.addListener(this);
	
	// print received messages to the console
	midiIn.setVerbose(true);
}	
//----------------------------------------------------------
void ofApp::midibiz(){
	for(unsigned int i = 0; i < midiMessages.size(); ++i) {

		ofxMidiMessage &message = midiMessages[i];
	
		if(message.status < MIDI_SYSEX) {
			//text << "chan: " << message.channel;
            if(message.status == MIDI_CONTROL_CHANGE) {
                
                //How to Midi Map
                //uncomment the line that says cout<<message control etc
                //run the code and look down in the console
                //when u move a knob on yr controller keep track of the number that shows up
                //that is the cc value of the knob
                //then go down to the part labled 'MIDIMAPZONE'
                //and change the numbers for each if message.control== statement to the values
                //on yr controller
                
                 // cout << "message.control"<< message.control<< endl;
                 // cout << "message.value"<< message.value<< endl;
 
                //MIDIMAPZONE
                //these are mostly all set to output bipolor controls at this moment (ranging from -1.0 to 1.0)
                //if u uncomment the second line on each of these if statements that will switch thems to unipolor
                //controls (ranging from 0.0to 1.0) if  you prefer
              
                if(message.control==43){
					if(message.value==127){/*turn something on*/}
					if(message.value==0){/*turn somethin off*/}
                }
                //nanokontrol2 controls
                 //c1 maps to fb0 lumakey
                if(message.control==16){
                   
					if(control_switch==0){	
						if(p_lock_0_switch==1){
							p_lock[0][p_lock_increment]=message.value/127.0f;
						}              
                    }
                    
                    if(control_switch==1){
						fftLow[0]=message.value/127.0f;
					}
					
					if(control_switch==2){
						fftMid[0]=message.value/127.0f;
					}
					
					if(control_switch==3){
						fftHigh[0]=message.value/127.0f;
					}
                }
                
               if(message.control==17){
                   
                   if(control_switch==0){
						if(p_lock_0_switch==1){
							p_lock[1][p_lock_increment]=(message.value-63)/63.0f;
						}
                    }
                    
                    if(control_switch==1){
						fftLow[1]=(message.value-63.0f)/63.0f;
					}
					
					if(control_switch==2){
						fftMid[1]=(message.value-63.0f)/63.0f;
					}
					
					if(control_switch==3){
						fftHigh[1]=(message.value-63.0f)/63.0f;
					}
                }
        
                
             
            }
		}
	}
}
//--------------------------------------------------------------
void ofApp::keyPressed(int key) {
	if (key == ')') {fft.setNormalize(true);}
	
}
//--------------------------------------------------------------
void ofApp::keyReleased(int key) {
}

