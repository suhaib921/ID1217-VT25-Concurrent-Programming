/*javac UnisexBathroomSimulation.java
  java UnisexBathroomSimulation
 */
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Random;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

class BathroomMonitor {
    private int menInside = 0;
    private int womenInside = 0;
    private int waitingMen = 0;
    private int waitingWomen = 0;
    private boolean turnForMen = true;

    public synchronized void manEnter(String name) {
        logEvent(name, "wants to enter (man)");
        waitingMen++;
        while ((womenInside > 0 || (!turnForMen && waitingWomen > 0)) && !Thread.currentThread().isInterrupted()) {
            try {
                wait(5000); // Timeout after 5 seconds.
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }
        waitingMen--;
        menInside++;
        logEntry(name, "man");
        if (menInside == 1) {
            turnForMen = true;
        }
    }

    public synchronized void manExit(String name) {
        menInside--;
        logExit(name, "man");
        if (menInside == 0 && waitingWomen > 0) {
            turnForMen = false;
        }
        notify(); // Notify only one waiting thread.
    }

    public synchronized void womanEnter(String name) {
        logEvent(name, "wants to enter bathroom");
        waitingWomen++;
        while ((menInside > 0 || (turnForMen && waitingMen > 0)) && !Thread.currentThread().isInterrupted()) {
            try {
                wait(5000); // Timeout after 5 seconds.
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }
        waitingWomen--;
        womenInside++;
        logEntry(name, "woman");
        if (womenInside == 1) {
            turnForMen = false;
        }
    }

    public synchronized void womanExit(String name) {
        womenInside--;
        logExit(name, "woman");
        if (womenInside == 0 && waitingMen > 0) {
            turnForMen = true;
        }
        notify(); // Notify only one waiting thread.
    }

    private void logEvent(String name, String message) {
        SimpleDateFormat sdf = new SimpleDateFormat("HH:mm:ss.SSS");
        String timeStamp = sdf.format(new Date());
        System.out.println(timeStamp + " [" + name + "] " + message);
    }

    private void logEntry(String name, String gender) {
        logEvent(name, "entered the bathroom (" + gender + ")");
    }

    private void logExit(String name, String gender) {
        logEvent(name, "exiting the bathroom (" + gender + ")");
    }
}

class Person extends Thread {
    private static final Random random = new Random();
    private BathroomMonitor monitor;

    public Person(BathroomMonitor monitor, String name) {
        super(name);
        this.monitor = monitor;
    }

    @Override
    public void run() {
        try {
            Thread.sleep(random.nextInt(2000)); // Simulate delay before wanting to enter.
            if (getName().startsWith("Man")) {
                monitor.manEnter(getName());
            } else {
                monitor.womanEnter(getName());
            }
            Thread.sleep(random.nextInt(2000) + 1000); // Simulate time spent in the bathroom.
            if (getName().startsWith("Man")) {
                monitor.manExit(getName());
            } else {
                monitor.womanExit(getName());
            }
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
    }
}

public class UnisexBathroomSimulation {
    public static void main(String[] args) {
        BathroomMonitor monitor = new BathroomMonitor();
        int numMen = 5;
        int numWomen = 5;
        Thread[] threads = new Thread[numMen + numWomen];

        for (int i = 0; i < numMen + numWomen; i++) {
            if (i < numMen) {
                threads[i] = new Person(monitor, "Man-" + (i + 1));
            } else {
                threads[i] = new Person(monitor, "Woman-" + (i - numMen + 1));
            }
        }

        List<Thread> threadList = Arrays.asList(threads);
        Collections.shuffle(threadList);
        threadList.toArray(threads);

        for (Thread t : threads) {
            t.start();
        }

        for (Thread t : threads) {
            try {
                t.join();
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }
    }
}