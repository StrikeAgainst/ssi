// MainOnline.cpp
// author: Johannes Wagner <wagner@hcm-lab.de>
// created: 2007/10/11
// Copyright (C) University of Augsburg, Lab for Human Centered Multimedia
//
// *************************************************************************************************
//
// This file is part of Social Signal Interpretation (SSI) developed at the 
// Lab for Human Centered Multimedia of the University of Augsburg
//
// This library is free software; you can redistribute itand/or
// modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation; either
// version 3 of the License, or any laterversion.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FORA PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public
// License along withthis library; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
//
//*************************************************************************************************

#include "ssi.h"
#include "ssimodel.h"
#include "ssiml/include/ssiml.h"
using namespace ssi;

#ifdef USE_SSI_LEAK_DETECTOR
	#include "SSI_LeakWatcher.h"
	#ifdef _DEBUG
		#define new DEBUG_NEW
		#undef THIS_FILE
		static char THIS_FILE[] = __FILE__;
	#endif
#endif

bool ex_gesture (void *args); 
bool ex_smooth(void *args);
bool ex_events(void *args);
bool ex_pipeline(void *args);

int main () {

#ifdef USE_SSI_LEAK_DETECTOR
	{
#endif

	ssi_print ("%s\n\nbuild version: %s\n\n", SSI_COPYRIGHT, SSI_VERSION);

	Factory::RegisterDLL ("frame");
	Factory::RegisterDLL ("event");
	Factory::RegisterDLL ("ioput");
	Factory::RegisterDLL ("mouse");
	Factory::RegisterDLL ("model");
	Factory::RegisterDLL ("graphic");
	Factory::RegisterDLL ("signal");
	Factory::RegisterDLL ("control");

#if SSI_RANDOM_LEGACY_FLAG
	ssi_random_seed();
#endif

	Exsemble exsemble;
	exsemble.console(0, 0, 650, 800);
	exsemble.add(&ex_gesture, 0, "GESTURE", "Online gesture recognizer");
	exsemble.add(&ex_smooth, 0, "SMOOTH", "Online decision smoothing");
	exsemble.add(&ex_events, 0, "EVENT", "Online recognition from events");
	exsemble.add(&ex_pipeline, 0, "PIPELINE", "Create annotations from a pipeline.");
	exsemble.show();

	Factory::Clear ();

#ifdef USE_SSI_LEAK_DETECTOR
	}
	_CrtDumpMemoryLeaks();
#endif
	
	return 0;
}

bool ex_gesture (void *args) {

	ITheFramework *frame = Factory::GetFramework ();

	Decorator *decorator = ssi_create (Decorator, 0, true);
	frame->AddDecorator(decorator);

	ITheEventBoard *board = Factory::GetEventBoard ();

	SampleList sample_list;
	unsigned int iterations, class_num;
	char class_name[1024];

	// mouse
	Mouse *mouse = ssi_create (Mouse, "mouse", true);
	mouse->getOptions()->mask = Mouse::LEFT;
	ITransformable *cursor_p = frame->AddProvider(mouse, SSI_MOUSE_CURSOR_PROVIDER_NAME);
	ITransformable *button_p = frame->AddProvider(mouse, SSI_MOUSE_BUTTON_PROVIDER_NAME);
	frame->AddSensor(mouse);

	// trigger
	ZeroEventSender *ezero = ssi_create (ZeroEventSender, "ezero", true);
	ezero->getOptions()->mindur = 0.2;
	ezero->getOptions()->setAddress("button@click");
	frame->AddConsumer(button_p, ezero, "0.25s");	
	board->RegisterSender(*ezero);
	
	EventConsumer *trigger = ssi_create (EventConsumer, 0, true);	
	board->RegisterListener(*trigger, ezero->getEventAddress());

	// plot
	SignalPainter *plot = ssi_create_id (SignalPainter, 0, "plot");
	plot->getOptions()->setTitle("mouse");
	trigger->AddConsumer(cursor_p, plot);

	// record	
	ssi_print("\n");
	ssi_print_off ("number of class> ");
	scanf ("%d", &class_num);
	ssi_print_off("iterations> ");
	scanf ("%d", &iterations);
	Collector *collector = ssi_create (Collector, 0, true);
	collector->getOptions()->setUserName("user");
	collector->getOptions()->iter = iterations;
	collector->setSampleList(sample_list);	
	ssi_print("\n");
	for (unsigned int i = 0; i < class_num; i++) {
		ssi_print_off("name of class %d from %d> ", i + 1, class_num);
		scanf ("%s", class_name);
		collector->getOptions()->addClassName(class_name);
	}
	trigger->AddConsumer(cursor_p, collector);	
	ssi_print("\n");

	// run

	decorator->add("console", 0, 0, 650, 800);
	decorator->add("plot*", 650, 0, 400, 400);
	decorator->add("monitor*", 650, 400, 400, 400);

	board->Start();
	frame->Start();
	ssi_print("\n");
	ssi_print_off ("Perform gesture while pressing left mouse button..\n\n");
	collector->wait();
	frame->Stop();
	board->Stop();
	trigger->clear();

	// view
	sample_list.print (stdout);
	
	// create, eval and save model
	{
		ssi_print_off("create trainer..\n");
		Dollar$1 *model = ssi_create (Dollar$1, 0, true);
		Trainer trainer (model);

		ssi_print_off("evaluate trainer..\n");
		Evaluation eval;
		eval.evalLOO (&trainer, sample_list);
		eval.print ();

		ssi_print_off("train classifier..\n");
		trainer.release();
		trainer.train (sample_list);
		trainer.save ("gesture.model");
	}

	Trainer trainer;
	Trainer::Load (trainer, "gesture.model");	

	// test
	ssi_print_off("start testing..\n");
	Classifier *classifier = ssi_create (Classifier, 0, true);
	classifier->setTrainer(&trainer);	
	classifier->getOptions()->console = true;
	trigger->AddConsumer(cursor_p, classifier);
	trigger->AddConsumer(cursor_p, plot);

	decorator->add("console", 0, 0, 650, 800);
	decorator->add("plot*", 650, 0, 400, 400);
	decorator->add("monitor*", 650, 400, 400, 400);

	board->Start();
	frame->Start();
	classifier->wait();
	frame->Stop();
	board->Stop();
	frame->Clear();
	board->Clear();

	return true;
}

struct fake_tuple_sender_arg {
	ITheEventBoard *b;
	ssi_event_t *e;
	ssi_size_t ms;
	ssi_size_t n;
};

void fake_tuple_sender(void *ptr) {
	fake_tuple_sender_arg *arg = ssi_pcast(fake_tuple_sender_arg, ptr);
	ssi_event_map_t *tuple = ssi_pcast(ssi_event_map_t, arg->e->ptr);
	Randomf random(0,1);
	for (ssi_size_t i = 0; i < arg->n; i++) {
		tuple[i].value = random.next();
	}
	arg->b->update(*arg->e);
	ssi_sleep(arg->ms);
}

bool ex_smooth(void *args) {

	ITheFramework *frame = Factory::GetFramework();

	Decorator *decorator = ssi_create (Decorator, 0, true);
	frame->AddDecorator(decorator);

	ITheEventBoard *board = Factory::GetEventBoard();

	ssi_size_t n_classes = 4;	
	ssi_event_t e;
	ssi_event_init(e, SSI_ETYPE_MAP, Factory::AddString("decision"), Factory::AddString("faked"));
	ssi_event_adjust(e, sizeof(ssi_event_map_t) * n_classes);
	ssi_event_map_t *tuple = ssi_pcast(ssi_event_map_t, e.ptr);
	ssi_char_t string[100];
	for (ssi_size_t i = 0; i < n_classes; i++) {
		ssi_sprint(string, "%c", ssi_cast(char, 'A' + i));
		tuple[i].id = Factory::AddString(string);
	}
	fake_tuple_sender_arg arg;
	arg.e = &e;
	arg.b = board;
	arg.ms = 3000;
	arg.n = n_classes;

	EventPainter *paint;
	DecisionSmoother *smoother;

	paint = ssi_create_id (EventPainter, 0, "plot");
	paint->getOptions()->type = PaintBars::TYPE::BAR_POS;
	paint->getOptions()->fix = 1.0f;
	paint->getOptions()->autoscale = false;
	paint->getOptions()->global = true;
	board->RegisterListener(*paint, "faked@decision");

	smoother = ssi_create(DecisionSmoother, "smoother", true);
	smoother->getOptions()->update_ms = 100;
	smoother->getOptions()->decay = 0.05f;
	smoother->getOptions()->speed = 0.2f;
	board->RegisterListener(*smoother, "faked@decision");
	board->RegisterSender(*smoother);

	paint = ssi_create_id (EventPainter, 0, "plot");
	paint->getOptions()->type = PaintBars::TYPE::BAR_POS;
	paint->getOptions()->fix = 1.0f;
	paint->getOptions()->autoscale = false;
	paint->getOptions()->global = true;
	board->RegisterListener(*paint, smoother->getEventAddress());

	smoother = ssi_create_id(DecisionSmoother, 0, "smoother");
	smoother->getOptions()->update_ms = 0;
	smoother->getOptions()->average = true;	
	smoother->getOptions()->setEventName("smoothed(avg)");
	board->RegisterListener(*smoother, "faked@decision");
	board->RegisterSender(*smoother);

	paint = ssi_create_id(EventPainter, 0, "plot");
	paint->getOptions()->type = PaintBars::TYPE::BAR_POS;
	paint->getOptions()->fix = 1.0f;
	paint->getOptions()->autoscale = false;
	paint->getOptions()->global = true;
	board->RegisterListener(*paint, smoother->getEventAddress());

	ControlButton *resetButton = ssi_create_id(ControlButton, 0, "control");
	resetButton->getOptions()->setLabel("RESET");
	frame->AddRunnable(resetButton);

	decorator->add("console", 0, 0, 650, 800);
	decorator->add("plot*", 1, 0, 650, 0, 400, 700);
	decorator->add("control*", 650, 700, 400, 100);

	board->Start();
	frame->Start();

	RunAsThread thread(&fake_tuple_sender, &arg);
	thread.start();

	frame->Wait();
	thread.stop();

	frame->Stop();
	board->Stop();
	frame->Clear();
	board->Clear();

	ssi_event_destroy(e);

	return true;

}

struct fake_floats_sender_arg {
	ITheEventBoard *b;
	ssi_event_t *e;
	ssi_size_t ms;
	ssi_size_t n;
};

void fake_floats_sender(void *ptr) {
	fake_floats_sender_arg *arg = ssi_pcast(fake_floats_sender_arg, ptr);
	ssi_real_t *floats = ssi_pcast(ssi_real_t, arg->e->ptr);
	Randomf random(0, 1);
	for (ssi_size_t i = 0; i < arg->n; i++) {
		floats[i] = random.next();
	}
	arg->b->update(*arg->e);
	ssi_sleep(arg->ms);
}

bool ex_events(void *args) {

	ITheFramework *frame = Factory::GetFramework();

	Decorator *decorator = ssi_create (Decorator, 0, true);
	frame->AddDecorator(decorator);

	ITheEventBoard *board = Factory::GetEventBoard();

	ssi_size_t n_classes = 4;
	ssi_size_t n_samples = 25;
	ssi_size_t n_streams = 1;
	ssi_real_t distr[][3] = { 0.3f, 0.3f, 0.2f, 0.3f, 0.6f, 0.2f, 0.6f, 0.3f, 0.2f, 0.6f, 0.6f, 0.2f };
	SampleList samples;
	ModelTools::CreateTestSamples(samples, n_classes, n_samples, n_streams, distr, "user");

	ssi_event_t e;
	ssi_event_init(e, SSI_ETYPE_TUPLE, Factory::AddString("faked"), Factory::AddString("features"), 0, 0, 2 * sizeof(ssi_real_t));
	fake_floats_sender_arg arg;
	arg.e = &e;
	arg.b = board;
	arg.ms = 3000;
	arg.n = 2;
	
	SVM *svm = ssi_create(SVM, 0, true);
	Trainer trainer(svm);
	trainer.train(samples);

	Classifier *classifier = ssi_create(Classifier, 0, true);
	classifier->setTrainer(&trainer);
	classifier->getOptions()->console = true;
	board->RegisterListener(*classifier, "features@faked");

	decorator->add("console", 0, 0, 650, 800);
	decorator->add("plot*", 650, 0, 400, 400);
	decorator->add("monitor*", 650, 400, 400, 400);

	board->Start();
	frame->Start();
	
	RunAsThread thread(&fake_floats_sender, &arg);
	thread.start();
	frame->Wait();
	thread.stop();

	frame->Stop();
	board->Stop();
	frame->Clear();
	board->Clear();

	return true;
}


bool ex_pipeline(void *args)
{
	ITheFramework *frame = Factory::GetFramework();
	ITheEventBoard *board = Factory::GetEventBoard();

	Decorator *decorator = ssi_create(Decorator, 0, true);
	frame->AddDecorator(decorator);

	Mouse *mouse = ssi_create(Mouse, 0, true);
	mouse->getOptions()->mask = Mouse::LEFT;
	mouse->getOptions()->sendEvent = true;
	mouse->getOptions()->setAddress("click@mouse");
	ITransformable *cursor_t = frame->AddProvider(mouse, SSI_MOUSE_CURSOR_PROVIDER_NAME);
	ITransformable *button_t = frame->AddProvider(mouse, SSI_MOUSE_BUTTON_PROVIDER_NAME);
	frame->AddSensor(mouse);
	board->RegisterSender(*mouse);

	FileAnnotationWriter *writer = 0;

	Annotation annod;	
	annod.setDiscreteScheme("discrete");
	writer = ssi_create(FileAnnotationWriter, 0, true);
	writer->setAnnotation(&annod);	
	writer->getOptions()->addUnkownLabel = true;
	writer->getOptions()->setDefaultLabel("click");
	writer->getOptions()->forceDefaultLabel = true;
	board->RegisterListener(*writer, mouse->getEventAddress());

	Annotation annoc;
	annoc.setContinuousScheme("continuous", cursor_t->getSampleRate(), 0, 1);
	writer = ssi_create(FileAnnotationWriter, 0, true);
	writer->setAnnotation(&annoc);		
	writer->getOptions()->streamConfidenceIndex = 1;
	writer->getOptions()->streamScoreIndex = 0;
	frame->AddConsumer(cursor_t, writer, "1.0s");

	SignalPainter *painter = ssi_create_id(SignalPainter, 0, "plot");
	painter->getOptions()->setTitle("CURSOR");
	frame->AddConsumer(cursor_t, painter, "1.0s");

	EventMonitor *monitor = ssi_create_id(EventMonitor, 0, "monitor");
	board->RegisterListener(*monitor);

	decorator->add("console", 0, 0, 650, 800);
	decorator->add("plot*", 650, 0, 400, 400);
	decorator->add("monitor*", 650, 400, 400, 400);

	board->Start();
	frame->Start();
	frame->Wait();
	frame->Stop();
	board->Stop();
	frame->Clear();
	board->Clear();

	annod.save("discrete", File::ASCII);
	annoc.save("continuous", File::ASCII);

	return true;
}